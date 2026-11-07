#pragma once

// Runs sample imports (WAV load -> resample -> effects -> 1-bit quantizer) on a background thread, so the
// editor does not freeze while a long sample is rendered. Jobs are processed one at a time, in order.
// Results are collected with TakeResults() from the UI/main thread (e.g. in OnIdle).

#include <condition_variable>
#include <deque>
#include <functional>
#include <mutex>
#include <string>
#include <thread>
#include <utility>
#include <vector>

template <class Job, class Result>
class SampleImportWorker
{
public:
  using ImportFn = std::function<Result(const Job&)>;

  explicit SampleImportWorker(ImportFn fn)
    : mImport(std::move(fn))
  {
  }

  ~SampleImportWorker() { Shutdown(); }

  SampleImportWorker(const SampleImportWorker&) = delete;
  SampleImportWorker& operator=(const SampleImportWorker&) = delete;

  void Enqueue(Job job)
  {
    std::lock_guard<std::mutex> lock(mMutex);
    if (mStopping)
      return;
    mJobs.push_back(std::move(job));
    if (!mThread.joinable())
      mThread = std::thread([this] { Run(); });
    mCondition.notify_one();
  }

  // Non-blocking: returns all results finished since the last call.
  std::vector<Result> TakeResults()
  {
    std::lock_guard<std::mutex> lock(mMutex);
    std::vector<Result> out;
    out.swap(mResults);
    return out;
  }

  // True while jobs are queued or running.
  bool Busy() const
  {
    std::lock_guard<std::mutex> lock(mMutex);
    return !mJobs.empty() || mRunning;
  }

  // Stops after the job in progress (queued jobs are dropped) and joins the thread.
  void Shutdown()
  {
    {
      std::lock_guard<std::mutex> lock(mMutex);
      mStopping = true;
      mJobs.clear();
      mCondition.notify_one();
    }
    if (mThread.joinable())
      mThread.join();
  }

private:
  void Run()
  {
    std::unique_lock<std::mutex> lock(mMutex);
    while (true)
    {
      mCondition.wait(lock, [this] { return mStopping || !mJobs.empty(); });
      if (mStopping)
        return;

      Job job = std::move(mJobs.front());
      mJobs.pop_front();
      mRunning = true;

      lock.unlock();
      Result result = mImport(job);
      lock.lock();

      mRunning = false;
      mResults.push_back(std::move(result));
    }
  }

  ImportFn mImport;
  mutable std::mutex mMutex;
  std::condition_variable mCondition;
  std::deque<Job> mJobs;
  std::vector<Result> mResults;
  bool mRunning = false;
  bool mStopping = false;
  std::thread mThread;
};
