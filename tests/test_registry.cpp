#include "includes.h"

#include <atomic>
#include <condition_variable>
#include <mutex>
#include <thread>

static const char *const tested_logger_name = "null_logger";
static const char *const tested_logger_name2 = "null_logger2";

// NOLINTBEGIN(cppcoreguidelines-avoid-non-const-global-variables)
bool flag1 = false;
std::condition_variable flag1_cv;
std::mutex flag1_m;
bool flag2 = false;
std::condition_variable flag2_cv;
std::mutex flag2_m;
// NOLINTEND(cppcoreguidelines-avoid-non-const-global-variables)

#ifndef SPDLOG_NO_EXCEPTIONS
TEST_CASE("register_drop", "[registry]")
{
    spdlog::drop_all();
    spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name);
    REQUIRE(spdlog::get(tested_logger_name) != nullptr);
    // Throw if registering existing name
    REQUIRE_THROWS_AS(spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name), spdlog::spdlog_ex);
}

TEST_CASE("explicit register", "[registry]")
{
    spdlog::drop_all();
    auto logger = std::make_shared<spdlog::logger>(tested_logger_name, std::make_shared<spdlog::sinks::null_sink_st>());
    spdlog::register_logger(logger);
    REQUIRE(spdlog::get(tested_logger_name) != nullptr);
    // Throw if registering existing name
    REQUIRE_THROWS_AS(spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name), spdlog::spdlog_ex);
}
#endif

TEST_CASE("apply_all", "[registry]")
{
    spdlog::drop_all();
    auto logger = std::make_shared<spdlog::logger>(tested_logger_name, std::make_shared<spdlog::sinks::null_sink_st>());
    spdlog::register_logger(logger);
    auto logger2 = std::make_shared<spdlog::logger>(tested_logger_name2, std::make_shared<spdlog::sinks::null_sink_st>());
    spdlog::register_logger(logger2);

    int counter = 0;
    spdlog::apply_all([&counter](std::shared_ptr<spdlog::logger>) { counter++; });
    REQUIRE(counter == 2);

    counter = 0;
    spdlog::drop(tested_logger_name2);
    spdlog::apply_all([&counter](std::shared_ptr<spdlog::logger> l) {
        REQUIRE(l->name() == tested_logger_name);
        counter++;
    });
    REQUIRE(counter == 1);
}

TEST_CASE("drop", "[registry]")
{
    spdlog::drop_all();
    spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name);
    spdlog::drop(tested_logger_name);
    REQUIRE_FALSE(spdlog::get(tested_logger_name));
}

TEST_CASE("drop-default", "[registry]")
{
    spdlog::set_default_logger(spdlog::null_logger_st(tested_logger_name));
    spdlog::drop(tested_logger_name);
    REQUIRE_FALSE(spdlog::default_logger());
    REQUIRE_FALSE(spdlog::get(tested_logger_name));
}

TEST_CASE("drop_all", "[registry]")
{
    spdlog::drop_all();
    spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name);
    spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name2);
    spdlog::drop_all();
    REQUIRE_FALSE(spdlog::get(tested_logger_name));
    REQUIRE_FALSE(spdlog::get(tested_logger_name2));
    REQUIRE_FALSE(spdlog::default_logger());
}

TEST_CASE("drop non existing", "[registry]")
{
    spdlog::drop_all();
    spdlog::create<spdlog::sinks::null_sink_mt>(tested_logger_name);
    spdlog::drop("some_name");
    REQUIRE_FALSE(spdlog::get("some_name"));
    REQUIRE(spdlog::get(tested_logger_name));
    spdlog::drop_all();
}

TEST_CASE("default logger", "[registry]")
{
    spdlog::drop_all();
    spdlog::set_default_logger(spdlog::null_logger_st(tested_logger_name));
    REQUIRE(spdlog::get(tested_logger_name) == spdlog::default_logger());
    spdlog::drop_all();
}

TEST_CASE("set_default_logger(nullptr)", "[registry]")
{
    spdlog::set_default_logger(nullptr);
    REQUIRE_FALSE(spdlog::default_logger());
}

TEST_CASE("disable automatic registration", "[registry]")
{
    // set some global parameters
    spdlog::level::level_enum log_level = spdlog::level::level_enum::warn;
    spdlog::set_level(log_level);
    // but disable automatic registration
    spdlog::set_automatic_registration(false);
    auto logger1 = spdlog::create<spdlog::sinks::daily_file_sink_st>(tested_logger_name, SPDLOG_FILENAME_T("filename"), 11, 59);
    auto logger2 = spdlog::create_async<spdlog::sinks::stdout_color_sink_mt>(tested_logger_name2);
    // loggers should not be part of the registry
    REQUIRE_FALSE(spdlog::get(tested_logger_name));
    REQUIRE_FALSE(spdlog::get(tested_logger_name2));
    // but make sure they are still initialized according to global defaults
    REQUIRE(logger1->level() == log_level);
    REQUIRE(logger2->level() == log_level);
    spdlog::set_level(spdlog::level::info);
    spdlog::set_automatic_registration(true);
}

TEST_CASE("per-thread default logger", "[registry]")
{
    spdlog::drop_all();
    auto logger = std::make_shared<spdlog::logger>(tested_logger_name, std::make_shared<spdlog::sinks::null_sink_st>());
    auto logger2 = std::make_shared<spdlog::logger>(tested_logger_name2, std::make_shared<spdlog::sinks::null_sink_st>());

    std::thread thread1([=] {
        spdlog::set_default_logger(logger);

        // notify main thread that default logger has been changed
        {
            std::lock_guard<std::mutex> lock(flag1_m);
            flag1 = true;
        }
        flag1_cv.notify_one();

        // wait for signal from main thread before terminating
        std::unique_lock<std::mutex> lock(flag1_m);
        flag1_cv.wait(lock, [&]{ return !flag1; });
    });

    std::thread thread2([=] {
        spdlog::set_default_logger(logger2);

        // notify main thread that default logger has been changed
        {
            std::lock_guard<std::mutex> lock(flag2_m);
            flag2 = true;
        }
        flag2_cv.notify_one();

        // wait for signal from main thread before terminating
        std::unique_lock<std::mutex> lock(flag2_m);
        flag2_cv.wait(lock, [&]{ return !flag2; });
    });

    // wait for thread1 to signal that the default logger has been changed
    {
        std::unique_lock<std::mutex> lock1(flag1_m);
        flag1_cv.wait(lock1, [&]{ return flag1; });
    }

#ifdef SPDLOG_PER_THREAD_DEFAULT_LOGGER
    REQUIRE(spdlog::default_logger() == nullptr);
#else
    REQUIRE((spdlog::default_logger()->name() == tested_logger_name || spdlog::default_logger()->name() == tested_logger_name2));
#endif

    // wait for thread2 to signal that the default logger has been changed
    {
        std::unique_lock<std::mutex> lock2(flag2_m);
        flag2_cv.wait(lock2, [&]{ return flag2; });
    }

#ifdef SPDLOG_PER_THREAD_DEFAULT_LOGGER
    REQUIRE(spdlog::default_logger() == nullptr);
#else
    REQUIRE((spdlog::default_logger()->name() == tested_logger_name || spdlog::default_logger()->name() == tested_logger_name2));
#endif

    // signal thread1 & thread2 that they may terminate
    {
        std::lock_guard<std::mutex> lock(flag1_m);
        flag1 = false;
    }
    flag1_cv.notify_one();
    {
        std::lock_guard<std::mutex> lock(flag2_m);
        flag2 = false;
    }
    flag2_cv.notify_one();

    thread1.join();
    thread2.join();

#ifdef SPDLOG_PER_THREAD_DEFAULT_LOGGER
    REQUIRE(spdlog::default_logger() == nullptr);
#else
    REQUIRE((spdlog::default_logger()->name() == tested_logger_name || spdlog::default_logger()->name() == tested_logger_name2));
#endif
}
