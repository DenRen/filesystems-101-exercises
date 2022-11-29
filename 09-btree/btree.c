#include <solution.h>
#include <stdlib.h>

#include "btree.hpp"

struct btree : public BTree<int>
{};

struct btree* btree_alloc(unsigned int L)
{
	return new btree{ 2 * L};
}

void btree_free(struct btree *t)
{
	delete t;
}

void btree_insert(struct btree *t, int x)
{
	t->insert(x);
}

void btree_delete(struct btree *t, int x)
{
	t->remove(x);
	t->draw();
}

bool btree_contains(struct btree *t, int x)
{
	return t->contain(x).node != nullptr;
}

struct btree_iter : BTree<int>::Iterator
{
};

struct btree_iter* btree_iter_start(struct btree *t)
{
	return new btree_iter{ t->begin() };
}

void btree_iter_end(struct btree_iter *i)
{
	delete i;
}

bool btree_iter_next(struct btree_iter *i, int *x)
{
	auto& it = *i;

	if (it.is_end())
		return false;

	*x = *it;
	++it;

	return true;
}
