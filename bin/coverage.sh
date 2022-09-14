#!/bin/bash

task_list="base_env_test base_socket_test base_session_multi_bind_test base_session_connect_test base_session_rst_test base_session_flow_test base_stress_pingpong stress_pingpong stress_weak_network"

for task in ${task_list};
do
  ./$task
  if [ $? -ne 0 ]; then 
    echo "$task has error"
    exit $?
  else
    echo "$task test success"
  fi
  echo ""
  echo ""
done

echo "coverage.sh finish"
