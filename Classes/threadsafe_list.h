#pragma once
#include <mutex>
#include <condition_variable>
#include <memory>

template<typename T>
class threadsafe_list
{
private:
	struct node
	{
		std::mutex m;
		std::shared_ptr<T> data;
		std::unique_ptr<node> next;
		node() :
			next()

		{}

		node(T&& value) :
			data(std::make_shared<T>(std::move(value)))
		{}
	};

	node head;
	std::mutex tail_mutex;
	node* tail;
	std::condition_variable data_cond;
	int _size;
private:
	node* get_tail()
	{
		std::lock_guard<std::mutex> tail_lock(tail_mutex);
		return tail;
	}
	std::unique_ptr<node> pop_head()
	{
		std::unique_ptr<node> old_head = std::move(head.next);
		head.next = std::move(old_head->next);
		return old_head;
	}
	std::unique_lock<std::mutex> wait_for_data()
	{
		std::unique_lock<std::mutex> head_lock(head.m);
		data_cond.wait(head_lock, [&] { return (head.next.get() != get_tail()); });
		return std::move(head_lock);
	}
	std::unique_ptr<node> wait_pop_head()
	{
		std::unique_lock<std::mutex> head_lock(wait_for_data());
		return pop_head();
	}
	std::unique_ptr<node> try_pop_head()
	{
		std::unique_lock<std::mutex> head_lock(head.m);
		if (head.next.get() == get_tail())
		{
			return nullptr;
		}
		return pop_head();
	}

public:
	threadsafe_list():
		tail(new node)
	{
		head.next = std::unique_ptr<node>(tail);
		printf("---- tail =%p head.next =%p \n", (tail), head.next.get());
	}
	~threadsafe_list()
	{
		//remove_if([](node const&) { return true; });
	}
	threadsafe_list(threadsafe_list const& other) = delete;
	threadsafe_list& operator=(threadsafe_list const& other) = delete;

	void push_front(T& value)
	{
		std::unique_ptr<node> new_node(new node(std::move(value)));
		std::lock_guard<std::mutex> lk(head.m);
		new_node->next = std::move(head.next);
		head.next = std::move(new_node);
		data_cond.notify_one();
	}
	void push_back(T& value)
	{
		std::lock_guard<std::mutex> lk(tail_mutex);
		tail->data = std::make_shared<T>(std::move(value));
		auto old_tail = std::move(tail);
		tail = new node();
		old_tail->next = std::unique_ptr<node>(tail);
		data_cond.notify_one();
	}

	std::shared_ptr<T> try_pop()
	{
		std::unique_ptr<node> const old_head = try_pop_head();
		printf("node() this = %p\n", old_head.get());
		return old_head ? old_head->data : std::shared_ptr<T>();
	}
	std::shared_ptr<T> wait_and_pop()
	{
		std::unique_ptr<node> const old_head = wait_pop_head();
		return old_head->data;
	}

	template<typename Predicate>
	void remove_if(Predicate p)
	{
		node* current = &head;
		std::unique_lock<std::mutex> lk(current->m);
		while (node* const next = current->next.get())
		{
			std::unique_lock<std::mutex> next_lk(next->m);
			if (p(*next->data))  // 18
			{
				std::unique_ptr<node> old_next = std::move(current->next);
				current->next = std::move(next->next);
				next_lk.unlock();
			}
			else
			{
				lk.unlock();
				current = next;
				lk = std::move(next_lk);
			}
		}
	}


	bool empty()
	{
		std::lock_guard<std::mutex> head_lock(head.m);
		return (head.next.get() == get_tail());
	}
};
