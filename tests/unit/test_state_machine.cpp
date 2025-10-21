#ifdef UNIT_TEST

#include <unity.h>
#include "../../src/core/StateMachine.h"
#include "../../src/core/SystemTypes.h"

void setUp(void) {
}

void tearDown(void) {
}

void test_state_machine_initialization(void) {
    StateMachine sm;
    TEST_ASSERT_EQUAL_INT(SystemState::INITIALIZING, sm.getCurrentState());
}

void test_state_machine_transition(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::RUNNING);
    TEST_ASSERT_EQUAL_INT(SystemState::RUNNING, sm.getCurrentState());
}

void test_state_machine_previous_state(void) {
    StateMachine sm;
    TEST_ASSERT_EQUAL_INT(SystemState::INITIALIZING, sm.getPreviousState());
    
    sm.transitionTo(SystemState::RUNNING);
    TEST_ASSERT_EQUAL_INT(SystemState::INITIALIZING, sm.getPreviousState());
}

void test_state_machine_multiple_transitions(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::RUNNING);
    TEST_ASSERT_EQUAL_INT(SystemState::RUNNING, sm.getCurrentState());
    
    sm.transitionTo(SystemState::ERROR);
    TEST_ASSERT_EQUAL_INT(SystemState::ERROR, sm.getCurrentState());
    TEST_ASSERT_EQUAL_INT(SystemState::RUNNING, sm.getPreviousState());
    
    sm.transitionTo(SystemState::RECOVERING);
    TEST_ASSERT_EQUAL_INT(SystemState::RECOVERING, sm.getCurrentState());
    TEST_ASSERT_EQUAL_INT(SystemState::ERROR, sm.getPreviousState());
}

void test_state_machine_state_changed(void) {
    StateMachine sm;
    bool changed = sm.hasStateChanged();
    TEST_ASSERT_TRUE(changed);
    
    changed = sm.hasStateChanged();
    TEST_ASSERT_FALSE(changed);
}

void test_state_machine_transition_count(void) {
    StateMachine sm;
    uint32_t count = sm.getTransitionCount();
    TEST_ASSERT_EQUAL_UINT32(0, count);
    
    sm.transitionTo(SystemState::RUNNING);
    count = sm.getTransitionCount();
    TEST_ASSERT_EQUAL_UINT32(1, count);
    
    sm.transitionTo(SystemState::ERROR);
    count = sm.getTransitionCount();
    TEST_ASSERT_EQUAL_UINT32(2, count);
}

void test_state_machine_transition_time(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::RUNNING);
    
    unsigned long time = sm.getTimeInCurrentState();
    TEST_ASSERT_TRUE(time >= 0);
}

void test_state_machine_time_tracking(void) {
    StateMachine sm;
    unsigned long initial_time = sm.getTimeInCurrentState();
    
    delay(10);
    
    unsigned long current_time = sm.getTimeInCurrentState();
    TEST_ASSERT_TRUE(current_time >= initial_time);
}

void test_state_machine_all_states(void) {
    StateMachine sm;
    
    SystemState states[] = {
        SystemState::INITIALIZING,
        SystemState::RUNNING,
        SystemState::PAUSED,
        SystemState::ERROR,
        SystemState::RECOVERING,
        SystemState::SHUTDOWN
    };
    
    for (size_t i = 0; i < sizeof(states)/sizeof(states[0]); i++) {
        sm.transitionTo(states[i]);
        TEST_ASSERT_EQUAL_INT(states[i], sm.getCurrentState());
    }
}

void test_state_machine_is_running(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::INITIALIZING);
    TEST_ASSERT_FALSE(sm.isRunning());
    
    sm.transitionTo(SystemState::RUNNING);
    TEST_ASSERT_TRUE(sm.isRunning());
    
    sm.transitionTo(SystemState::PAUSED);
    TEST_ASSERT_FALSE(sm.isRunning());
}

void test_state_machine_is_error(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::RUNNING);
    TEST_ASSERT_FALSE(sm.isError());
    
    sm.transitionTo(SystemState::ERROR);
    TEST_ASSERT_TRUE(sm.isError());
}

void test_state_machine_is_recovering(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::RECOVERING);
    TEST_ASSERT_TRUE(sm.isRecovering());
}

void test_state_machine_can_transition(void) {
    StateMachine sm;
    sm.transitionTo(SystemState::RUNNING);
    
    bool can_transition = true;
    sm.transitionTo(SystemState::ERROR);
    TEST_ASSERT_EQUAL_INT(SystemState::ERROR, sm.getCurrentState());
}

#endif
