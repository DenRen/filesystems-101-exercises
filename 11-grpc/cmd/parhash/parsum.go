package main

import (
	"context"
	"encoding/hex"
	"fmt"
	"io"
	"log"
	"os"

	"github.com/pkg/errors"
	"github.com/spf13/cobra"
	"google.golang.org/grpc"

	parhashpb "fs101ex/pkg/gen/parhashsvc"
)

var sumFlags struct {
	addr string
}

var sumCmd = cobra.Command{
	Use:   "parsum <file0> <file1> ...",
	Short: "compute sha256 sum of short files",
	Run:   parsum,
}

func init() {
	f := sumCmd.Flags()
	f.StringVar(&sumFlags.addr, "addr", "", "address of the hasher service")

	rootCmd.AddCommand(&sumCmd)
}

func parsum(cmd *cobra.Command, args []string) {
	ctx := context.Background()

	if sumFlags.addr == "" {
		log.Fatalf("--addr must be provided")
	}

	conn, err := grpc.Dial(sumFlags.addr,
		grpc.WithInsecure(), /* allow non-TLS connections */
	)
	if err != nil {
		log.Fatalf("failed to connect to %q: %v", sumFlags.addr, err)
	}
	defer conn.Close()

	client := parhashpb.NewParallelHashSvcClient(conn)

	num_files := len(args)
	fmt.Println("Number files: ", num_files)

	datas := make([][]byte, num_files)
	for i_file := range args {
		file_name := args[i_file]

		file, err := os.Open(file_name)
		if err != nil {
			return
		}

		st, err := file.Stat()
		if st.Size() > 1024*1024 {
			fmt.Println(errors.Errorf("file \"%q\" is too big", file_name))
			return
		}

		datas[i_file], err = io.ReadAll(file)
		if err != nil {
			return
		}
	}

	resp, err := client.ParallelHash(ctx, &parhashpb.ParHashReq{Data: datas})
	if err != nil || resp == nil {
		fmt.Println(err)
		return
	}

	for i_file := range resp.Hashes {
		fmt.Printf("%s  %s\n", args[i_file], hex.EncodeToString(resp.Hashes[i_file]))
	}
}
