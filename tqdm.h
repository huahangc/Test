#ifndef TQDM_H
#define TQDM_H

#include <chrono>
#include <cstdlib>
#include <iomanip>
#include <iostream>
#include <iterator>
#include <sstream>
#include <string>

#ifdef __unix__
#include <sys/ioctl.h>
#include <unistd.h>
#endif

namespace tqdm {

// ============================================================
// ProgressBar — 核心进度条类
// ============================================================

class ProgressBar {
public:
    explicit ProgressBar(size_t total, std::string prefix = "",
                         std::ostream& os = std::cerr)
        : total_(total)
        , n_(0)
        , prefix_(std::move(prefix))
        , os_(os)
        , closed_(false)
        , start_time_(std::chrono::steady_clock::now())
        , last_update_time_(start_time_)
        , min_update_interval_(0.1)
    {}

    ~ProgressBar() {
        if (!closed_) close();
    }

    ProgressBar(const ProgressBar&) = delete;
    ProgressBar& operator=(const ProgressBar&) = delete;

    ProgressBar(ProgressBar&& other) noexcept
        : total_(other.total_)
        , n_(other.n_)
        , prefix_(std::move(other.prefix_))
        , os_(other.os_)
        , closed_(other.closed_)
        , start_time_(other.start_time_)
        , last_update_time_(other.last_update_time_)
        , min_update_interval_(other.min_update_interval_)
    {
        other.closed_ = true;
    }

    ProgressBar& operator=(ProgressBar&& other) noexcept {
        if (this != &other) {
            total_ = other.total_;
            n_ = other.n_;
            prefix_ = std::move(other.prefix_);
            closed_ = other.closed_;
            start_time_ = other.start_time_;
            last_update_time_ = other.last_update_time_;
            min_update_interval_ = other.min_update_interval_;
            other.closed_ = true;
        }
        return *this;
    }

    void update(size_t n = 1) {
        n_ += n;
        refresh();
    }

    void set_prefix(std::string prefix) {
        prefix_ = std::move(prefix);
    }

    void close() {
        if (closed_) return;
        force_refresh();
        os_ << "\n" << std::flush;
        closed_ = true;
    }

    void reset() {
        n_ = 0;
        closed_ = false;
        start_time_ = std::chrono::steady_clock::now();
        last_update_time_ = start_time_;
    }

    size_t n() const { return n_; }
    size_t total() const { return total_; }

private:
    void refresh() {
        if (closed_) return;
        auto now = std::chrono::steady_clock::now();
        double elapsed_since_last =
            std::chrono::duration<double>(now - last_update_time_).count();
        if (elapsed_since_last < min_update_interval_ && n_ < total_) {
            return;
        }
        do_refresh(now);
    }

    void force_refresh() {
        if (closed_) return;
        do_refresh(std::chrono::steady_clock::now());
    }

    void do_refresh(std::chrono::steady_clock::time_point now) {
        int tw = get_terminal_width();
        double elapsed = std::chrono::duration<double>(now - start_time_).count();

        int pct = (total_ > 0) ? static_cast<int>((n_ * 100) / total_) : 0;
        if (pct > 100) pct = 100;

        double rate = 0.0;
        double eta = 0.0;
        if (elapsed > 0 && n_ > 0) {
            rate = static_cast<double>(n_) / elapsed;
            if (rate > 0 && total_ > n_) {
                eta = static_cast<double>(total_ - n_) / rate;
            }
        }

        // 构建各部分
        std::string left;
        if (!prefix_.empty()) left = prefix_ + " ";
        left += "|";

        std::ostringstream pct_ss, count_ss, right_ss;
        pct_ss << std::setw(3) << pct << "%";
        count_ss << n_ << "/" << total_;
        right_ss << "[" << format_time(elapsed) << "<" << format_time(eta)
                 << ", " << std::fixed << std::setprecision(2) << rate
                 << "it/s]";

        std::string pct_str = pct_ss.str();
        std::string count_str = count_ss.str();
        std::string right_str = right_ss.str();

        // 计算进度条可用宽度
        // left + bar + "|" + " " + pct + " " + count + " " + right
        int fixed_width = static_cast<int>(left.size()) + 1
                        + 1 + static_cast<int>(pct_str.size())
                        + 1 + static_cast<int>(count_str.size())
                        + 1 + static_cast<int>(right_str.size());
        int bar_width = tw - fixed_width;
        if (bar_width < 10) bar_width = 10;

        int filled = 0;
        if (total_ > 0) {
            filled = static_cast<int>((bar_width * n_) / total_);
            if (filled > bar_width) filled = bar_width;
        }

        // UTF-8: █ = \xe2\x96\x88, ░ = \xe2\x96\x91
        std::string bar_str;
        bar_str.reserve(bar_width * 3);
        for (int i = 0; i < filled; ++i) bar_str += "\xe2\x96\x88";
        for (int i = filled; i < bar_width; ++i) bar_str += "\xe2\x96\x91";

        os_ << "\r" << left << bar_str << "| " << pct_str << " "
            << count_str << " " << right_str << std::flush;
        last_update_time_ = now;
    }

    std::string format_time(double seconds) const {
        if (seconds < 0) return "00:00";
        auto total_secs = static_cast<size_t>(seconds);
        auto mins = total_secs / 60;
        auto secs = total_secs % 60;
        if (mins > 59) return "59:59+";
        std::ostringstream ss;
        ss << std::setw(2) << std::setfill('0') << mins << ":"
           << std::setw(2) << std::setfill('0') << secs;
        return ss.str();
    }

    int get_terminal_width() const {
#ifdef __unix__
        struct winsize w;
        if (ioctl(STDOUT_FILENO, TIOCGWINSZ, &w) == 0 && w.ws_col > 0) {
            return static_cast<int>(w.ws_col);
        }
#endif
        const char* cols = std::getenv("COLUMNS");
        if (cols) {
            int w = std::atoi(cols);
            if (w > 0) return w;
        }
        return 80;
    }

    size_t total_;
    size_t n_;
    std::string prefix_;
    std::ostream& os_;
    bool closed_;
    std::chrono::steady_clock::time_point start_time_;
    std::chrono::steady_clock::time_point last_update_time_;
    double min_update_interval_;
};

// ============================================================
// Iterator — 自动更新迭代器模板
// ============================================================

template <typename Iter>
class Iterator {
public:
    using iterator_category = typename std::iterator_traits<Iter>::iterator_category;
    using value_type = typename std::iterator_traits<Iter>::value_type;
    using difference_type = typename std::iterator_traits<Iter>::difference_type;
    using pointer = typename std::iterator_traits<Iter>::pointer;
    using reference = typename std::iterator_traits<Iter>::reference;

    Iterator(Iter it, ProgressBar* bar) : it_(it), bar_(bar) {}
    Iterator(const Iterator&) = default;
    Iterator& operator=(const Iterator&) = default;

    reference operator*() const { return *it_; }
    pointer operator->() const { return &(*it_); }

    Iterator& operator++() {
        ++it_;
        if (bar_) bar_->update();
        return *this;
    }

    Iterator operator++(int) {
        Iterator tmp = *this;
        ++(*this);
        return tmp;
    }

    bool operator==(const Iterator& other) const { return it_ == other.it_; }
    bool operator!=(const Iterator& other) const { return it_ != other.it_; }

private:
    Iter it_;
    ProgressBar* bar_;
};

// ============================================================
// TqdmContainer — 容器遍历包装器（持有引用）
// ============================================================

template <typename Container>
class TqdmContainer {
public:
    using iter_type = Iterator<typename Container::iterator>;

    TqdmContainer(Container& c, std::string prefix = "",
                  std::ostream& os = std::cerr)
        : container_(c), bar_(c.size(), std::move(prefix), os) {}

    iter_type begin() { return iter_type(container_.begin(), &bar_); }
    iter_type end() { return iter_type(container_.end(), nullptr); }

    TqdmContainer& set_prefix(std::string prefix) {
        bar_.set_prefix(std::move(prefix));
        return *this;
    }

private:
    Container& container_;
    ProgressBar bar_;
};

// ============================================================
// Range — 整数范围
// ============================================================

class Range {
public:
    class Iter {
    public:
        using iterator_category = std::forward_iterator_tag;
        using value_type = size_t;
        using difference_type = std::ptrdiff_t;
        using pointer = const size_t*;
        using reference = const size_t&;

        explicit Iter(size_t val) : val_(val) {}
        reference operator*() const { return val_; }
        pointer operator->() const { return &val_; }
        Iter& operator++() { ++val_; return *this; }
        Iter operator++(int) { Iter t = *this; ++val_; return t; }
        bool operator==(const Iter& o) const { return val_ == o.val_; }
        bool operator!=(const Iter& o) const { return val_ != o.val_; }
    private:
        size_t val_;
    };

    Range(size_t start, size_t stop) : start_(start), stop_(stop) {}
    Iter begin() const { return Iter(start_); }
    Iter end() const { return Iter(stop_); }
    size_t size() const { return stop_ - start_; }

private:
    size_t start_, stop_;
};

// ============================================================
// TqdmRange — 整数范围遍历包装器（拥有 Range 对象）
// ============================================================

class TqdmRange {
public:
    using iter_type = Iterator<Range::Iter>;

    TqdmRange(size_t start, size_t stop, std::string prefix = "",
              std::ostream& os = std::cerr)
        : range_(start, stop), bar_(range_.size(), std::move(prefix), os) {}

    iter_type begin() { return iter_type(range_.begin(), &bar_); }
    iter_type end() { return iter_type(range_.end(), nullptr); }

    TqdmRange& set_prefix(std::string prefix) {
        bar_.set_prefix(std::move(prefix));
        return *this;
    }

private:
    Range range_;
    ProgressBar bar_;
};

// ============================================================
// 自由函数：tqdm()
// ============================================================

// 遍历容器：tqdm(container, "prefix")
template <typename Container>
TqdmContainer<Container> tqdm(Container& c, std::string prefix = "",
                              std::ostream& os = std::cerr) {
    return TqdmContainer<Container>(c, std::move(prefix), os);
}

// 整数范围：tqdm(start, stop, "prefix")
inline TqdmRange tqdm(size_t start, size_t stop, std::string prefix = "",
                      std::ostream& os = std::cerr) {
    return TqdmRange(start, stop, std::move(prefix), os);
}

// 简写：tqdm(count, "prefix") 等价于 tqdm(0, count, "prefix")
inline TqdmRange tqdm(size_t count, std::string prefix = "",
                      std::ostream& os = std::cerr) {
    return TqdmRange(0, count, std::move(prefix), os);
}

} // namespace tqdm

#endif // TQDM_H
