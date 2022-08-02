#include "Test.hpp"
#include "main.h"
#include <unistd.h>
#include <exception>
#include <cmsis_os.h>
#include <cstring>
#include "logger.h"
#include <cstdarg>
#include <cctype>
#include "FreeRTOS.h"
#include "task.h"
#include "rlm3-string.h"


LOGGER_ZONE(TEST);


TestCaseListItem* TestCaseListItem::g_head = nullptr;
TestCaseListItem** TestCaseListItem::g_tail = &g_head;

TestHelperListItem* TestHelperListItem::g_head = nullptr;
TestHelperListItem** TestHelperListItem::g_tail = &g_head;

static int g_test_depth = 0;
static bool g_test_failure = false;
TaskHandle_t g_testing_thread_id = 0;

#define LOG_BUFFER_SIZE 256
static char g_log_buffer[LOG_BUFFER_SIZE];
static volatile size_t g_log_head = 0;
static volatile size_t g_log_tail = 0;


static bool IsIRQ()
{
	return (__get_IPSR() != 0U);
}

static size_t AdvanceLogCursor(size_t cursor)
{
	if (cursor >= LOG_BUFFER_SIZE - 1)
		return 0;
	return cursor + 1;
}

static void TestFlushLogBuffer()
{
	if (IsIRQ())
		return;

	size_t head = g_log_head;
	size_t tail = g_log_tail;
	while (tail != head)
	{
		ITM_SendChar(g_log_buffer[tail]);
		tail = AdvanceLogCursor(tail);
	}
	g_log_tail = tail;
}

static void TestWriteToLogBufferFormatFn(void* ignore, char c)
{
	size_t head = g_log_head;
	size_t next = AdvanceLogCursor(head);

	// If there is room in the buffer, add this character.
	if (next != g_log_tail)
	{
		g_log_buffer[head] = c;
		g_log_head = next;
	}
}

static void TestWriteToTestConsoleFn(void* ignore, char c)
{
	ITM_SendChar(c);
}

static void TestFormat(const char* format, ...)
{
	TestFlushLogBuffer();
	va_list args;
	va_start(args, format);
	RLM3_FnVFormat(TestWriteToTestConsoleFn, nullptr, format, args);
	va_end(args);
}

extern void logger_format_message(LoggerLevel level, const char* zone, const char* format, ...)
{
	// NOTE: This is not threadsafe.

	TickType_t tick_count = IsIRQ() ? xTaskGetTickCountFromISR() : xTaskGetTickCount();

	// Decide if output should go to buffer or directly to the test console.
	RLM3_Format_Fn format_fn;
	if (IsIRQ() && level < LOGGER_LEVEL_ERROR)
	{
		format_fn = TestWriteToLogBufferFormatFn;
	}
	else
	{
		TestFlushLogBuffer();
		format_fn = TestWriteToTestConsoleFn;

	}

	// Write out this message.
	va_list args;
	va_start(args, format);
	RLM3_FnFormat(format_fn, nullptr, "%u %s %s ", (int)tick_count, ToString(level), zone);
	RLM3_FnVFormat(format_fn, nullptr, format, args);
	RLM3_FnFormat(format_fn, nullptr, "\n");
	va_end(args);
}

static void terminate_fn()
{
	LOG_FATAL("terminate called");
	while (1);
}

namespace __cxxabiv1 {
	std::terminate_handler __terminate_handler = terminate_fn;
}

extern "C" void RLM3_Main(void)
{
	TestCaseListItem::RunAll();
}

TestCaseListItem::TestCaseListItem(TestCase test, const char* name, const char* file, long line)
	: m_test(test)
	, m_name(name)
	, m_file(file)
	, m_line(line)
	, m_next(nullptr)
	, m_prev(g_tail)
{
	*g_tail = this;
	g_tail = &this->m_next;
}

TestCaseListItem::~TestCaseListItem()
{
	*m_prev = m_next;
}

bool TestCaseListItem::Run()
{
	TestHelperListItem::Run(TestHelperListItem::SETUP);
	g_test_depth++;
	g_test_failure = false;
	bool test_passed = false;
	g_testing_thread_id = xTaskGetCurrentTaskHandle();
	try
	{
		TestHelperListItem::Run(TestHelperListItem::START);
		(m_test)();
		TestHelperListItem::Run(TestHelperListItem::FINISH);
		test_passed = !g_test_failure;
	}
	catch (const AssertFailedException& e)
	{
	}
	catch (const std::exception& e)
	{
		const char* name = typeid(e).name();
		TestFormat("FAILED - Test failed with exception %s.  Error: %s\n", name, e.what());
	}
	catch (...)
	{
		TestFlushLogBuffer();
		TestFormat("FAILED - Test failed with unknown exception.\n");
	}
	g_test_depth--;
	g_test_failure = false;
	g_testing_thread_id = 0;
	TestHelperListItem::Run(TestHelperListItem::TEARDOWN);
	TestFlushLogBuffer();

	return test_passed;
}

bool TestCaseListItem::RunAll()
{
	TestFormat("== RUNNING TEST CASES ==\n");

	size_t total_test_count = 0;
	size_t passed_test_count = 0;

	for (TestCaseListItem* current_test = g_head; current_test != nullptr; current_test = current_test->m_next)
	{
		TestFormat("=== TEST: %s ===\n", current_test->m_name);

		total_test_count++;
		if (current_test->Run())
		{
			passed_test_count++;
		}
		else
		{
			TestFormat("=== TEST FAILED: %s File '%s' line %u ===\n", current_test->m_name, current_test->m_file, (unsigned int)current_test->m_line);
		}
	}

	TestFormat("== TEST SUMMARY ==\n");
	TestFormat("%d Total Test\n", total_test_count);
	TestFormat("%d Tests Passed\n", passed_test_count);
	if (total_test_count == passed_test_count)
	{
		TestFormat("== TESTS PASSED ==\n");
		TestFormat("EOT PASS\n");
		return true;
	}
	else
	{
		TestFormat("%d Failed Tests\n", total_test_count - passed_test_count);
		TestFormat("== TESTS FAILED ==\n");
		TestFormat("EOT FAIL\n");
		return false;
	}
}

TestHelperListItem::TestHelperListItem(HelperFn fn, Type type)
	: m_helper_fn(fn)
	, m_type(type)
	, m_next(nullptr)
	, m_prev(g_tail)
{
	*g_tail = this;
	g_tail = &this->m_next;
}

TestHelperListItem::~TestHelperListItem()
{
	*m_prev = m_next;
}

void TestHelperListItem::Run(Type type)
{
	for (TestHelperListItem* cursor = g_head; cursor != nullptr; cursor = cursor->m_next)
		if (cursor->m_type == type)
			(*cursor->m_helper_fn)();
}

static bool is_destructor(const char* function_name)
{
	return (std::strstr(function_name, "::~") != nullptr);
}

extern void NotifyAssertFailed(const char* file, long line, const char* function, const char* message, ...)
{
	bool throw_error = true;
	const char* failure_type = "ASSERT FAILED";

	if (g_test_depth == 0)
	{
		failure_type = "ASSERT FAILED OUTSIDE TESTS";
		throw_error = false;
	}

	if (g_test_depth > 1)
	{
		failure_type = "NESTED ASSERT FAILED";
	}

	if (g_testing_thread_id != xTaskGetCurrentTaskHandle())
	{
		failure_type = "ASSERT FAILED IN SECONDARY THREAD";
		g_test_failure = true;
		throw_error = false;
	}

	if (std::uncaught_exception())
	{
		failure_type = "ASSERT FAILED IN EXCEPTION PROCESSING";
		g_test_failure = true;
		throw_error = false;
	}

	if (is_destructor(function))
	{
		failure_type = "ASSERT FAILED IN DESTRUCTOR";
		g_test_failure = true;
		throw_error = false;
	}

	// Write out any previous interrupt logs.
	TestFlushLogBuffer();

	// Write out this message.
	va_list args;
	va_start(args, message);
	RLM3_FnFormat(TestWriteToTestConsoleFn, nullptr, "%s:%ld:1: assert: ‘", file, line);
	RLM3_FnVFormat(TestWriteToTestConsoleFn, nullptr, message, args);
	RLM3_FnFormat(TestWriteToTestConsoleFn, nullptr, "‘ %s in function: %s\n", failure_type, function);
	va_end(args);

	if (throw_error)
	{
		throw AssertFailedException();
	}
}

extern "C" void __cxa_pure_virtual()
{
	LOG_FATAL("pure virtual");
	while(1);
}

void* operator new(size_t size)
{
	return pvPortMalloc(size);
}

void operator delete(void* ptr)
{
	if (ptr != nullptr)
		vPortFree(ptr);
}

extern "C" void _exit(int result)
{
	LOG_FATAL("exit called");
	while(1);
}

extern "C" pid_t _getpid(void)
{
	return 0;
}

extern "C" int _kill(pid_t pid, int sig)
{
	return 0;
}

extern void* DANGER_sbrk_was_linked_into_application();

extern "C" void* _sbrk(ptrdiff_t incr)
{
	return DANGER_sbrk_was_linked_into_application();
}

extern "C" void* _malloc_r(struct _reent*, size_t size)
{
	return pvPortMalloc(size);
}

extern "C" void _free_r(struct _reent*, void *p)
{
	vPortFree(p);
}
