package parhash

import (
	"context"
	"fmt"
	"log"
	"net"
	"sync"

	"github.com/pkg/errors"
	"golang.org/x/sync/semaphore"
	"google.golang.org/grpc"

	hashpb "fs101ex/pkg/gen/hashsvc"
	parhashpb "fs101ex/pkg/gen/parhashsvc"
	"fs101ex/pkg/workgroup"
)

type Config struct {
	ListenAddr   string
	BackendAddrs []string
	Concurrency  int
}

// Implement a server that responds to ParallelHash()
// as declared in /proto/parhash.proto.
//
// The implementation of ParallelHash() must not hash the content
// of buffers on its own. Instead, it must send buffers to backends
// to compute hashes. Buffers must be fanned out to backends in the
// round-robin fashion.
//
// For example, suppose that 2 backends are configured and ParallelHash()
// is called to compute hashes of 5 buffers. In this case it may assign
// buffers to backends in this way:
//
//	backend 0: buffers 0, 2, and 4,
//	backend 1: buffers 1 and 3.
//
// Requests to hash individual buffers must be issued concurrently.
// Goroutines that issue them must run within /pkg/workgroup/Wg. The
// concurrency within workgroups must be limited by Server.sem.
//
// WARNING: requests to ParallelHash() may be concurrent, too.
// Make sure that the round-robin fanout works in that case too,
// and evenly distributes the load across backends.
type Server struct {
	conf Config

	stop context.CancelFunc
	l    net.Listener
	wg   sync.WaitGroup

	sem *semaphore.Weighted

	rr_ctr_mutex sync.Mutex
	rr_ctr       int
	backends     []hashpb.HashSvcClient
}

func New(conf Config) *Server {
	return &Server{
		conf: conf,
		sem:  semaphore.NewWeighted(int64(conf.Concurrency)),
	}
}

func (s *Server) ConnectToBackends() (err error) {
	ctx := context.Background()

	s.backends = make([]hashpb.HashSvcClient, len(s.conf.BackendAddrs))
	var (
		wg = workgroup.New(workgroup.Config{Sem: semaphore.NewWeighted(8)})
	)

	for index, address := range s.conf.BackendAddrs {
		i := index
		addr := address

		wg.Go(ctx, func(ctx context.Context) (err error) {
			conn, err := grpc.Dial(addr,
				grpc.WithInsecure(), /* allow non-TLS connections */
			)
			if err != nil {
				log.Fatalf("Failed to connect to %q: %v", addr, err)
				return nil
			}

			s.backends[i] = hashpb.NewHashSvcClient(conn)
			fmt.Println("Connected to ", addr, " ", s.backends[i])

			return nil
		})
	}

	if err := wg.Wait(); err != nil {
		log.Fatalf("Failed to connect to backends: %v", err)
	}

	return nil
}

func (s *Server) Start(ctx context.Context) (err error) {
	defer func() { err = errors.Wrapf(err, "Start()") }()

	ctx, s.stop = context.WithCancel(ctx)

	s.l, err = net.Listen("tcp", s.conf.ListenAddr)
	if err != nil {
		return err
	}

	srv := grpc.NewServer()
	parhashpb.RegisterParallelHashSvcServer(srv, s)
	s.ConnectToBackends()
	s.rr_ctr = 0

	s.wg.Add(2)
	go func() {
		defer s.wg.Done()

		srv.Serve(s.l)
	}()
	go func() {
		defer s.wg.Done()

		<-ctx.Done()
		s.l.Close()
	}()

	return nil
}

func (s *Server) ListenAddr() string {
	return s.l.Addr().String()
}

func (s *Server) Stop() {
	s.stop()
	s.wg.Wait()
}

func (s *Server) ParallelHash(ctx context.Context, req *parhashpb.ParHashReq) (resp *parhashpb.ParHashResp, err error) {
	num_data := len(req.Data)
	if num_data == 0 {
		return nil, errors.Errorf("where data?")
	}

	num_backends := len(s.backends)
	wg := workgroup.New(workgroup.Config{Sem: semaphore.NewWeighted(int64(s.conf.Concurrency))})

	hashes := make([][]byte, num_data)

	for index := range req.Data {
		i := index

		s.rr_ctr_mutex.Lock()
		s.rr_ctr++
		if s.rr_ctr == num_backends {
			s.rr_ctr = 0
		}

		rr_index := s.rr_ctr
		s.rr_ctr_mutex.Unlock()

		wg.Go(ctx, func(ctx context.Context) (err error) {
			resp, err := s.backends[rr_index].Hash(ctx, &hashpb.HashReq{Data: req.Data[i]})
			if err != nil {
				return errors.Errorf("Backend error: ", err)
			}

			hashes[i] = resp.Hash
			return nil
		})
	}

	if err := wg.Wait(); err != nil {
		log.Fatalf("Failed to parhash files: %v", err)
	}

	return &parhashpb.ParHashResp{Hashes: hashes}, nil
}
