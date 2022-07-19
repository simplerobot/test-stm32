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


LOGGER_ZONE(TEST);


TestCaseListItem* TestCaseListItem::g_head = nullptr;
TestCaseListItem** TestCaseListItem::g_tail = &g_head;

TestHelperListItem* TestHelperListItem::g_head = nullptr;
TestHelperListItem** TestHelperListItem::g_tail = &g_head;

static int g_test_depth = 0;
static bool g_test_failure = false;
TaskHandle_t g_testing_thread_id = 0;


static void ITM_SendCharSafe(char c)
{
	if (c == '\\')
	{
		ITM_SendChar('\\');
		ITM_SendChar('\\');
	}
	else if (' ' <= c && c <= '~')
	{
		ITM_SendChar(c);
	}
	else if (c == '\r')
	{
		ITM_SendChar('\\');
		ITM_SendChar('r');
	}
	else if (c == '\n')
	{
		ITM_SendChar('\\');
		ITM_SendChar('n');
	}
	else
	{
		ITM_SendChar('?');
	}
}

static void ITM_SendString(const char* str)
{
  if (str == NULL)
	  str = "(null)";
  while (*str != 0)
	  ITM_SendCharSafe(*(str++));
}

static void ITM_SendUInt(uint32_t x)
{
  char buffer[10];
  int count = 0;
  do
  {
    buffer[count++] = '0' + (x % 10);
    x /= 10;
  } while (x != 0);
  for (int i = 0; i < count; i++)
    ITM_SendChar(buffer[count - i - 1]);
}

static void ITM_SendInt(int32_t x)
{
  if (x < 0)
  {
    ITM_SendChar('-');
    x = -x;
  }
  ITM_SendUInt((uint32_t)x);
}

static char ToHexDigit(uint32_t x, bool uppercase)
{
	if (x >= 0 && x <= 9)
		return '0' + x;
	if (x >= 10 && x <= 15)
		return (uppercase ? 'A' : 'a') + x - 10;
	return '?';
}
static void ITM_SendHex(uint32_t x, bool uppercase)
{
  size_t i = 1;
  while (i < 8 && ((x >> (32 - 4 * i)) & 0x0F) == 0)
    i++;
  for (; i <= 8; i++)
    ITM_SendChar(ToHexDigit((x >> (32 - 4 * i)) & 0x0F, uppercase));
}

static void ITM_VFormat(const char* format, va_list args)
{
	while (*format != 0)
	{
		char c = *(format++);
		if (c == '%')
		{
			char f = *(format++);
			while ((f >= '0' && f <= '9') || (f == 'z') || (f == 'l'))
				f = *(format++);
			if (f == 's')
				ITM_SendString(va_arg(args, const char*));
			else if (f == 'd')
				ITM_SendInt(va_arg(args, int));
			else if (f == 'u')
				ITM_SendUInt(va_arg(args, unsigned int));
			else if (f == 'x')
				ITM_SendHex(va_arg(args, unsigned int), false);
			else if (f == 'X')
				ITM_SendHex(va_arg(args, unsigned int), true);
			else if (f == 'c')
				ITM_SendCharSafe((char)va_arg(args, int));
			else if (f == '%')
				ITM_SendChar(f);
			else
			{
				ITM_SendString("[unsupported format ");
				ITM_SendCharSafe(f);
				ITM_SendChar(']');
				return;
			}
		}
		else
			ITM_SendCharSafe(c);
	}
}

static void ITM_Format(const char* format, ...)
{
	va_list args;
	va_start(args, format);
	ITM_VFormat(format, args);
	va_end(args);
}

extern void logger_format_message(LoggerLevel level, const char* zone, const char* format, ...)
{
	ITM_SendUInt(xTaskGetTickCount());
	ITM_SendChar(' ');
	ITM_SendString(ToString(level));
	ITM_SendChar(' ');
	ITM_SendString(zone);
	ITM_SendChar(' ');

	va_list args;
	va_start(args, format);
	ITM_VFormat(format, args);
	va_end(args);

	ITM_SendChar('\n');
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

class AssertFailedException
{
public:
	AssertFailedException() {}
};

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
		ITM_Format("FAILED - Test failed with exception %s.  Error: %s", name, e.what());
		ITM_SendChar('\n');
	}
	catch (...)
	{
		ITM_Format("FAILED - Test failed with unknown exception.");
		ITM_SendChar('\n');
	}
	g_test_depth--;
	g_test_failure = false;
	g_testing_thread_id = 0;
	TestHelperListItem::Run(TestHelperListItem::TEARDOWN);

	return test_passed;
}

bool TestCaseListItem::RunAll()
{
	ITM_Format("== RUNNING TEST CASES ==");
	ITM_SendChar('\n');

	size_t total_test_count = 0;
	size_t passed_test_count = 0;

	for (TestCaseListItem* current_test = g_head; current_test != nullptr; current_test = current_test->m_next)
	{
		ITM_Format("=== TEST: %s ===", current_test->m_name);
		ITM_SendChar('\n');

		total_test_count++;

		if (current_test->Run())
		{
			passed_test_count++;
		}
		else
		{
			ITM_Format("=== TEST FAILED: %s File '%s' line %d ===", current_test->m_name, current_test->m_file, current_test->m_line);
			ITM_SendChar('\n');
		}
	}

	ITM_Format("== TEST SUMMARY ==");
	ITM_SendChar('\n');
	ITM_Format("%d Total Tests", total_test_count);
	ITM_SendChar('\n');
	ITM_Format("%d Tests Passed", passed_test_count);
	ITM_SendChar('\n');
	if (total_test_count == passed_test_count)
	{
		ITM_Format("== TESTS PASSED ==");
		ITM_SendChar('\n');
		ITM_Format("EOT PASS");
		ITM_SendChar('\n');
		return true;
	}
	else
	{
		ITM_Format("%d Failed Tests", total_test_count - passed_test_count);
		ITM_SendChar('\n');
		ITM_Format("== TESTS FAILED ==");
		ITM_SendChar('\n');
		ITM_Format("EOT FAIL");
		ITM_SendChar('\n');
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

static const char* ShortFileName(const char* filename)
{
	const char* result = filename;
	for (const char* cursor = filename; *cursor != 0; cursor++)
		if (*cursor == '/' || *cursor == '\\')
			result = cursor + 1;
	return result;
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

	va_list args;
	va_start(args, message);
	ITM_Format("%s '", failure_type);
	ITM_VFormat(message, args);
	ITM_Format("' %s %s:%d", function, ShortFileName(file), line);
	ITM_SendChar('\n');

	if (throw_error)
	{
		throw AssertFailedException();
	}
}

extern "C" void __cxa_pure_virtual()
{
	ITM_Format("pure virtual\n");
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
