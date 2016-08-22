#include "threadsafe_queue.h"

template<typename T>
threadsafe_queue<T>::threadsafe_queue() :
	head(new node)
	, tail(head.get())
{
}

template<typename T>
threadsafe_queue<T>::~threadsafe_queue()
{
}

template<typename T>
void threadsafe_queue<T>::push(T new_value)
{
	std::shared_ptr<T> new_data(std::make_shared<T>(std::move(new_value)));
	std::unique_ptr<node> p(new node);
	{
		std::lock_guard<std::mutex> tail_lock(tail_mutex);
		tail->data = new_data;
		node* const new_tail = p.get();
		tail->next = std::move(p);
		tail = new_tail;
	}
	data_cond.notify_one();
}

template<typename T>
threadsafe_queue<T>::node* threadsafe_queue<T>::get_tail()
{
	std::lock_guard<std::mutex> tail_lock(tail_mutex);
	return tail;
}

template<typename T>
std::unique_ptr<node> threadsafe_queue<T>::pop_head()
{

}