#ifndef UNITY_TEST_H
#define UNITY_TEST_H

#include "unity.h"

/* 测试函数声明 */
void setUp(void);
void tearDown(void);
void run_unity_tests(void);

/* 测试用例声明 */
void test_usart_communication(void);

#endif /* UNITY_TEST_H */

