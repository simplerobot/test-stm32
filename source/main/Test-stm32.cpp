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

#define LOG_INTERRUPT_BUFFER_SIZE 256
static char g_log_interrupt_buffer[LOG_INTERRUPT_BUFFER_SIZE];
static volatile size_t g_log_interrupt_head = 0;
static volatile size_t g_log_interrupt_tail = 0;


static void TestFormat(const char* format, ...) __attribute__ ((format (printf, 1, 2)));
static void TestVFormat(const char* format, va_list args) __attribute__ ((format (printf, 1, 0)));


static bool IsIRQ()
{
	return (__get_IPSR() != 0U);
}

static void TestWriteChar(char c)
{
	if (IsIRQ())
	{
		// Log to the interrupt buffer for later flushing.  We use temporary variables to avoid wrapping issues.
		size_t head = g_log_interrupt_head;
		size_t next = head + 1;
		if (next >= LOG_INTERRUPT_BUFFER_SIZE)
			next = 0;
		if (next != g_log_interrupt_tail)
		{
			g_log_interrupt_buffer[head] = c;
			g_log_interrupt_head = next;
		}
	}
	else
	{
		// Log directly to the test console.
		ITM_SendChar(c);
	}
}

static void TestWriteCharToFormat(void* ignore, char c)
{
	TestWriteChar(c);
}

static void TestFlushInterruptLogBuffer()
{
	// While there are characters in the interrupt queue, flush them.  We use temporary variables to avoid wrapping issues and starvation issues.
	size_t tail = g_log_interrupt_tail;
	while (g_log_interrupt_head != tail)
	{
		ITM_SendChar(g_log_interrupt_buffer[tail]);
		if (++tail >= LOG_INTERRUPT_BUFFER_SIZE)
			tail = 0;
	}
	g_log_interrupt_tail = tail;
}

static void TestFormat(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	RLM3_FnVFormat(TestWriteCharToFormat, nullptr, format, args);
	va_end(args);
}

static void TestVFormat(const char* format, va_list args)
{
	RLM3_FnVFormat(TestWriteCharToFormat, nullptr, format, args);
}

extern void logger_format_message(LoggerLevel level, const char* zone, const char* format, ...)
{
	// Flush any messages that were buffered from interrupts.
	if (!IsIRQ())
		TestFlushInterruptLogBuffer();

	TickType_t tick_count;
	if (!IsIRQ())
		tick_count = xTaskGetTickCount();
	else
		tick_count = xTaskGetTickCountFromISR();

	// Write out this message.
	TestFormat("%u %s %s ", (int)tick_count, ToString(level), zone);
	va_list args;
	va_start(args, format);
	TestVFormat(format, args);
	va_end(args);
	TestWriteChar('\n');
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
		TestFlushInterruptLogBuffer();
		TestHelperListItem::Run(TestHelperListItem::FINISH);
		TestFlushInterruptLogBuffer();
		test_passed = !g_test_failure;
	}
	catch (const AssertFailedException& e)
	{
	}
	catch (const std::exception& e)
	{
		const char* name = typeid(e).name();
		TestFlushInterruptLogBuffer();
		TestFormat("FAILED - Test failed with exception %s.  Error: %s\n", name, e.what());
	}
	catch (...)
	{
		TestFlushInterruptLogBuffer();
		TestFormat("FAILED - Test failed with unknown exception.\n");
	}
	g_test_depth--;
	g_test_failure = false;
	g_testing_thread_id = 0;
	TestHelperListItem::Run(TestHelperListItem::TEARDOWN);
	TestFlushInterruptLogBuffer();

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
	// Flush any messages that were buffered from interrupts.
	if (!IsIRQ())
		TestFlushInterruptLogBuffer();

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

	va_list args;
	va_start(args, message);
	TestFormat("%s:%ld:1: assert: ‘", file, line);
	TestVFormat(message, args);
	TestFormat("‘ %s ", failure_type);
	if (std::strncmp(function, "TEST_CASE_", 10) == 0)
		TestFormat("in test: TEST_CASE(%s)", function + 10);
	else
		TestFormat("in function: %s", function);
	TestFormat("\n");
	va_end(args);

	if (throw_error)
	{
		throw AssertFailedException();
	}
}

extern "C" void __cxa_pure_virtual()
{
	TestFormat("pure virtual\n");
	TestFlushInterruptLogBuffer();
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
	TestFlushInterruptLogBuffer();
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
