//
// Created by Yichen Yang on 9/15/19.
//

#include <iostream>
#include <string>
#include <random>
#include <vector>
#include <unordered_map>
#include <fstream>

using namespace std;

int main(){
    /// setup the log file name, test case chars
    vector<string> grep_item;
    vector<char> step_char = {'a', 'c', 'e', 'f', 'g', 'h', 'j', 'k'};
    int step_now = 0;
    int step_length = 8;
    unordered_map<string, vector<int>> grep_item_count;
    string curr_grep = "";
    vector<string> log_file_name = {"vm1_test.log", "vm2_test.log","vm3_test.log","vm4_test.log","vm5_test.log","vm6_test.log","vm7_test.log","vm8_test.log","vm9_test.log","vm10_test.log"};

    vector<vector<string>> logs;

    /// clean the environment
    for(int i = 0; i < 10; i++){
        remove(log_file_name[i].c_str());
    }

    for(int i = 0; i < 10; i++){
        vector<string> curr;
        logs.push_back(curr);
    }

    /// generate the random engine
    default_random_engine e;
    uniform_int_distribution<unsigned> u(20, 50);

    /// use random engine to generate random number of lines of different grep request
    /// this part will cover the patterns that are frequent and somewhat frequent
    /// it will grep some and almost all logs
    for(step_now = 0; step_now < step_length; step_now++){
        curr_grep += step_char[step_now];
        grep_item.push_back(curr_grep);
        vector<int> begin(10, 0);
        grep_item_count[curr_grep] = begin;

        for(int i = 0; i < 10; i++){
            int curr_rand;
            curr_rand = u(e);
            for(int j = 0; j < curr_rand; j++){
                logs[i].push_back(curr_grep);
            }
            for(const auto & p : grep_item){
                grep_item_count[p][i] += curr_rand;
            }
        }
    }

    /// generate fixed number of specific grep request
    string fixed_grep = "bcdefg";
    grep_item.push_back(fixed_grep);
    vector<int> fixed_grep_count(10, 33);
    grep_item_count[fixed_grep] = fixed_grep_count;
    for(int i = 0; i < 10; i++){
        for(int j = 0; j < 33; j++){
            logs[i].push_back(fixed_grep);
        }
    }

    /// generate fixed number of general use grep request
    fixed_grep = "b";
    grep_item.push_back(fixed_grep);
    vector<int> fixed_grep_count2(10, 88);
    grep_item_count[fixed_grep] = fixed_grep_count2;
    for(int i = 0; i < 10; i++){
        for(int j = 0; j < 55; j++){
            logs[i].push_back(fixed_grep);
        }
    }

    /// generate rare pattern, it will only appear once in the grep request
    fixed_grep = "zzzzzzzzz";
    grep_item.push_back(fixed_grep);
    vector<int> fixed_grep_count3(10, 1);
    grep_item_count[fixed_grep] = fixed_grep_count3;
    for(int i = 0; i < 10; i++){
        logs[i].push_back(fixed_grep);
    }

    /// generate the vmX_test.log files
    for(int i = 0 ; i < 3; i++){
        ofstream log_output(log_file_name[i]);
        int length = logs[i].size();
        for(int j = 0 ; j < length; j++){
            log_output << logs[i][j] << '\n';
        }
    }

    /// generate the test_answer file which contains the expected answer
    string log_answer_name = "test_answer";
    remove(log_answer_name.c_str());
    vector<vector<string>> log_answers;
    for(int i = 0; i < 10; i++){
        vector<string> curr;
        log_answers.push_back(curr);
    }

    /// generate the test_request file which contains all the requests corresponding to the answer
    string log_request_name = "test_request";
    remove(log_request_name.c_str());
    vector<string> log_requests;

    string curr_grep_request;
    string curr_grep_answer;
    int length = grep_item.size();
    for(int i = 0 ; i < length; i++){
        curr_grep_request = "grep '" + grep_item[i] + "' vm_test.log";
        log_requests.push_back(curr_grep_request);
        for(int j = 0 ; j < 3; j++){
            if(grep_item_count[grep_item[i]][j] > 1){
                curr_grep_answer = "From vm" + to_string(j + 1) + ": " + to_string(grep_item_count[grep_item[i]][j]) + " lines.";
                log_answers[j].push_back(curr_grep_answer);
            }else{
                curr_grep_answer = "From vm" + to_string(j + 1) + ": " + to_string(grep_item_count[grep_item[i]][j]) + " line.";
                log_answers[j].push_back(curr_grep_answer);
            }

        }
    }

    /// generate the most frequent request pattern and test the regular expression
    /// it will contain all the logs
    curr_grep_request = "grep -E '*' vm_test.log";
    log_requests.push_back(curr_grep_request);
    for(int j = 0 ; j < 3; j++){
        int curr_total_length = logs[j].size();
        curr_grep_answer = "From vm" + to_string(j + 1) + ": " + to_string(curr_total_length) + " lines.";
        log_answers[j].push_back(curr_grep_answer);
    }

    /// now write the request and answers to the files
    int request_length = log_requests.size();
    ofstream request_out;
    request_out.open(log_request_name);
    for(int i = 0 ; i < request_length; i++) {
        request_out << log_requests[i] << '\n';
    }
    request_out << "exit\n" ;
    request_out.close();

    int answer_length = log_answers[0].size();
    ofstream answer_out;
    answer_out.open(log_answer_name);
    for(int i = 0; i < answer_length; i++){
        for(int j = 0 ; j < 3; j++){
            answer_out << log_answers[j][i] << '\n';
        }
    }
    answer_out.close();

}
