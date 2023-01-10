// 每个操作符都由对应的操作符函数
// 因为，我们只需要提供operator new()/delete()即可
// 对象的析构和构造由编译器负责产生相应的代码进行调用，不需要我们操作
extern "C" void *malloc(unsigned int)
extern "C" void *free(void *);

void* operator new(unsigned int size) {
	return malloc(size);
}

void operator delete(void *p) {
	free(p);
}

void* operator new[] (unsigned int size) {
	return malloc(size)
}

void operator delete[](void *p) {
	free(p);
}
