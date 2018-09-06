#pragma once

enum class ProgressState
{
	InProgress,
	Completed,
	Failed
};

template<class T>
class AsyncFuture
{
public:
	void addOnCompletionListener(std::function<void(T&)> listener);
	virtual void onCompleted(T& value);

	bool didFail() const { return m_state == ProgressState::Failed; }
	bool isCompleted() const { return m_state == ProgressState::Completed; }

	T& getValue() { return m_value; }

	void setState(ProgressState state) { m_state = state; }
protected:
	T m_value;
	ProgressState m_state = ProgressState::InProgress;
	std::vector<std::function<void(T&)>> m_onCompletionListeners;
	std::mutex m_mutex;
};

template<class T>
void AsyncFuture<T>::onCompleted(T& value)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	m_value = value;
	m_state = ProgressState::Completed;
	for (auto& l : m_onCompletionListeners)
		l(m_value);
}

template<class T>
void AsyncFuture<T>::addOnCompletionListener(std::function<void(T&)> listener)
{
	std::lock_guard<std::mutex> lock(m_mutex);

	if (isCompleted())
		listener(m_value);
	else
		m_onCompletionListeners.push_back(listener);
}
