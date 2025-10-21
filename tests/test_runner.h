#ifndef TEST_RUNNER_H
#define TEST_RUNNER_H

#include <unity.h>

#ifdef UNIT_TEST
extern void test_audio_processor_initialization(void);
extern void test_audio_quality_levels(void);
extern void test_noise_reducer_initialization(void);
extern void test_agc_gain_calculation(void);
extern void test_vad_voice_detection(void);
extern void test_audio_buffer_write_read(void);
extern void test_audio_buffer_overflow_protection(void);
extern void test_rms_calculation(void);
extern void test_peak_calculation(void);
extern void test_audio_feature_enabling(void);
extern void test_safe_mode_toggling(void);
extern void test_processing_control(void);
extern void test_statistics_reset(void);

extern void test_network_quality_initialization(void);
extern void test_wifi_network_creation(void);
extern void test_multi_wifi_manager_initialization(void);
extern void test_multi_wifi_manager_add_network(void);
extern void test_multi_wifi_manager_multiple_networks(void);
extern void test_multi_wifi_manager_clear_networks(void);
extern void test_network_manager_initialization(void);
extern void test_network_manager_wifi_status(void);
extern void test_network_manager_server_status(void);
extern void test_network_manager_statistics_initialization(void);
extern void test_network_quality_metrics(void);
extern void test_network_manager_safe_mode(void);
extern void test_wifi_network_priority(void);
extern void test_network_manager_add_wifi_networks(void);
extern void test_network_stability_score(void);

extern void test_state_machine_initialization(void);
extern void test_state_machine_transition(void);
extern void test_state_machine_previous_state(void);
extern void test_state_machine_multiple_transitions(void);
extern void test_state_machine_state_changed(void);
extern void test_state_machine_transition_count(void);
extern void test_state_machine_transition_time(void);
extern void test_state_machine_time_tracking(void);
extern void test_state_machine_all_states(void);
extern void test_state_machine_is_running(void);
extern void test_state_machine_is_error(void);
extern void test_state_machine_is_recovering(void);
extern void test_state_machine_can_transition(void);
#endif

#ifdef INTEGRATION_TEST
extern void test_wifi_reconnection_basic(void);
extern void test_multi_wifi_failover(void);
extern void test_connection_quality_monitoring(void);
extern void test_wifi_reconnect_statistics(void);
extern void test_tcp_error_tracking(void);
extern void test_network_data_transfer(void);
extern void test_connection_validation(void);
extern void test_network_scan(void);
extern void test_bandwidth_estimation(void);
extern void test_connection_quality_test(void);
extern void test_available_networks_list(void);
extern void test_bytes_sent_tracking(void);
extern void test_bytes_received_tracking(void);
extern void test_server_reconnect_statistics(void);

extern void test_audio_stream_initialization(void);
extern void test_audio_buffer_management(void);
extern void test_audio_processing_pipeline(void);
extern void test_audio_quality_adaptation(void);
extern void test_voice_activity_during_streaming(void);
extern void test_noise_reduction_effectiveness(void);
extern void test_agc_during_streaming(void);
extern void test_audio_quality_score(void);
extern void test_audio_statistics_collection(void);
extern void test_input_output_levels(void);
extern void test_audio_i2s_health(void);
extern void test_audio_processor_safe_mode(void);
extern void test_audio_data_read_operation(void);
extern void test_audio_processing_control(void);
extern void test_audio_data_retry_mechanism(void);
#endif

#ifdef STRESS_TEST
extern void test_audio_buffer_allocation_cycles(void);
extern void test_audio_processor_initialization_cycles(void);
extern void test_noise_reducer_reinitialization(void);
extern void test_agc_continuous_processing(void);
extern void test_vad_continuous_voice_detection(void);
extern void test_memory_pool_stress(void);
extern void test_audio_buffer_circular_writes(void);
extern void test_audio_processing_extended_session(void);
extern void test_rapid_quality_level_changes(void);
extern void test_feature_toggle_stress(void);
extern void test_statistics_collection_stress(void);
#endif

#ifdef PERFORMANCE_TEST
extern void test_audio_processing_latency(void);
extern void test_vad_detection_latency(void);
extern void test_agc_processing_latency(void);
extern void test_noise_reduction_latency(void);
extern void test_buffer_read_latency(void);
extern void test_buffer_write_latency(void);
extern void test_rms_calculation_latency(void);
extern void test_peak_calculation_latency(void);
extern void test_quality_score_calculation_latency(void);
extern void test_statistics_retrieval_latency(void);

extern void test_audio_buffer_throughput(void);
extern void test_audio_processing_throughput(void);
extern void test_network_data_throughput_simulation(void);
extern void test_vad_processing_throughput(void);
extern void test_agc_processing_throughput(void);
extern void test_rms_calculation_throughput(void);
extern void test_peak_calculation_throughput(void);
#endif

#endif
