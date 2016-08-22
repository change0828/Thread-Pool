
#include <iostream>       // std::cout
#include <atomic>         // std::atomic
#include <thread>         // std::thread, std::this_thread::yield

std::atomic<int> foo = 0;
std::atomic<int> bar = 0;

void set_foo(int x)
{
	foo = x;
}

void copy_foo_to_bar()
{

	// ��� foo == 0������߳� yield,
	// �� foo == 0 ʱ, ʵ��Ҳ������������ת������,
	// ���Ҳ������ operator T() const �ĵ���.
	while (foo == 0) std::this_thread::yield();

	// ʵ�ʵ����� operator T() const, ��foo ǿ��ת���� int ����,
	// Ȼ����� operator=().
	bar = static_cast<int>(foo);
}

void print_bar()
{
	int x = 0;
	// ��� bar == 0������߳� yield,
	// �� bar == 0 ʱ, ʵ��Ҳ������������ת������,
	// ���Ҳ������ operator T() const �ĵ���.
	while (bar == 0)
	{
		std::this_thread::yield();
	}
	std::cout << "bar: " << bar << '\n';
}

int main()
{
	//std::thread first(print_bar);
	//std::thread second(set_foo, 10);
	//std::thread third(copy_foo_to_bar);

	//first.join();
	//second.join();
	//third.join();
	for (int level = 0; level <= 40; level++)
	{
		std::cout << " level : " << level << " floor(pow(level, 1.6) * 8.0f/10.0f)*10.0f : " << floor(pow(level, 1.6) * 8.0f / 10.0f)*10.0f << '\n';
	}
	return 0;
}