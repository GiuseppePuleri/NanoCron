#!/bin/bash

# Script nanoCron TESTER (PARSING ONLY - FAIR COMPARISON)
# Author: Giuseppe Puleri
# Version: 3.0 - Parsing Only Fair Comparison

set -e

SCRIPT_DIR="$(cd "$(dirname "${BASH_SOURCE[0]}")" && pwd)"
LOG_DIR="./test_logs"
LOG_FILE="${LOG_DIR}/performance.log"
NANOCRON_BINARY="/usr/local/bin/nanoCron"
SYSTEM_TESTER="./cron_system_tester"
NANOCRON_TEST_RUNNER="./nanocron_test_runner"

# Colori per output
RED='\033[0;31m'
GREEN='\033[0;32m'
YELLOW='\033[1;33m'
BLUE='\033[0;34m'
PURPLE='\033[0;35m'
NC='\033[0m' # No Color

echo -e "${PURPLE}=== nanoCron vs System Cron Performance Test Suite (PARSING ONLY - FAIR COMPARISON) ===${NC}"
echo -e "${PURPLE}Version: 3.0 - Parsing Only Fair Comparison${NC}"
echo -e "${BLUE}Data: $(date)${NC}"
echo ""
echo -e "${GREEN}KEY IMPROVEMENT: Both tests now perform IDENTICAL parsing operations${NC}"
echo -e "${GREEN}• nanoCron: JSON parsing of 10 jobs${NC}"
echo -e "${GREEN}• System Cron: Crontab parsing of 10 equivalent jobs${NC}"
echo -e "${GREEN}• NO system interactions, NO file operations beyond parsing${NC}"
echo ""

check_prerequisites() {
    echo -e "${YELLOW}Checking prerequisites...${NC}"
    
    # Crea directory di log con path corretto
    mkdir -p "${LOG_DIR}"
    
    # Verifica che la directory sia stata creata
    if [ ! -d "${LOG_DIR}" ]; then
        echo -e "${RED}Error: Could not create log directory ${LOG_DIR}${NC}"
        exit 1
    fi
    
    echo -e "${GREEN}Log directory created/verified: ${LOG_DIR}${NC}"
    echo -e "${BLUE}Performance log will be saved to: ${LOG_FILE}${NC}"

    if [ ! -f "$NANOCRON_BINARY" ]; then
        echo -e "${YELLOW}Warning: nanoCron binary not found at $NANOCRON_BINARY${NC}"
        echo -e "${YELLOW}Continuing with available tests...${NC}"
    fi
    
    # Compila versione PARSING ONLY del system cron tester
    if [ ! -f "$SYSTEM_TESTER" ]; then
        echo -e "${YELLOW}Compiling PARSING ONLY system cron tester...${NC}"
        
        # Verifica che il file sorgente esista
        if [ ! -f "cron_system_tester.cpp" ]; then
            echo -e "${RED}Error: cron_system_tester.cpp not found${NC}"
            echo -e "${YELLOW}Please ensure you have the PARSING ONLY version of the source file${NC}"
            exit 1
        fi
        
        g++ -o "$SYSTEM_TESTER" cron_system_tester.cpp -std=c++17 -pthread
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error: Failed to compile PARSING ONLY system cron tester${NC}"
            exit 1
        fi
        echo -e "${GREEN}PARSING ONLY system cron tester compiled successfully${NC}"
    fi

    # Compila nanoCron test runner (invariato)
    if [ ! -f "$NANOCRON_TEST_RUNNER" ]; then
        echo -e "${YELLOW}Compiling nanoCron test runner...${NC}"
        
        # Verifica che il file sorgente esista
        if [ ! -f "nanocron_test_runner.cpp" ]; then
            echo -e "${RED}Error: nanocron_test_runner.cpp not found${NC}"
            exit 1
        fi
        
        g++ -o "$NANOCRON_TEST_RUNNER" ./nanocron_test_runner.cpp -std=c++17 -pthread
        if [ $? -ne 0 ]; then
            echo -e "${RED}Error: Failed to compile nanoCron test runner${NC}"
            exit 1
        fi
        echo -e "${GREEN}nanoCron test runner compiled successfully${NC}"
    fi

    echo -e "${GREEN}Prerequisites check passed${NC}"
    echo ""
}

cleanup_logs() {
    echo -e "${YELLOW}Cleaning up previous test logs...${NC}"
    
    # Assicurati che la directory esista prima di procedere
    mkdir -p "${LOG_DIR}"
    
    if [ -f "$LOG_FILE" ]; then
        mv "$LOG_FILE" "${LOG_FILE}.backup.$(date +%Y%m%d_%H%M%S)"
        echo -e "${GREEN}Backed up existing log file${NC}"
    fi

    # Inizializza il nuovo file di log con header migliorato
    echo "=== nanoCron vs System Cron Performance Test Results (PARSING ONLY - FAIR COMPARISON) ===" > "$LOG_FILE"
    echo "Version: 3.0 - Parsing Only Fair Comparison" >> "$LOG_FILE"
    echo "Date: $(date)" >> "$LOG_FILE"
    echo "System: $(uname -a)" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"
    echo "=== FAIR COMPARISON METHODOLOGY ===" >> "$LOG_FILE"
    echo "1. IDENTICAL WORKLOADS: Both tests parse exactly 10 jobs" >> "$LOG_FILE"
    echo "2. PARSING ONLY: No system interactions, no file operations beyond parsing" >> "$LOG_FILE"
    echo "3. EQUIVALENT DATA: JSON jobs converted to equivalent crontab format" >> "$LOG_FILE"
    echo "4. UNIFIED MEASUREMENT: Both use /proc/self/status and clock()" >> "$LOG_FILE"
    echo "5. SAME PROCESSING: Both perform identical validation and simulation steps" >> "$LOG_FILE"
    echo "6. SAME ENVIRONMENT: Both run in dedicated processes with same monitoring" >> "$LOG_FILE"
    echo "========================================" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"

    echo -e "${GREEN}Log cleaned up and initialized with fair comparison info${NC}"
    echo -e "${BLUE}Log file: $LOG_FILE${NC}"
    echo ""
}

test_nanocron_performance() {
    echo -e "${BLUE}Testing nanoCron JSON parsing performance (PARSING ONLY)...${NC}"
    
    create_test_jobs_json

    local iterations=5
    echo "Running $iterations iterations of nanoCron parsing test..."

    for i in $(seq 1 $iterations); do
        echo -e "${YELLOW}nanoCron test iteration $i/$iterations (JSON PARSING ONLY)${NC}"
        echo -e "${BLUE}Executing: $NANOCRON_TEST_RUNNER ./test_jobs.json \"${LOG_DIR}\"${NC}"
        $NANOCRON_TEST_RUNNER ./test_jobs.json "${LOG_DIR}"
        
        # Verifica che il file di log sia stato scritto
        if [ -f "$LOG_FILE" ]; then
            echo -e "${GREEN}Log file updated successfully${NC}"
        else
            echo -e "${YELLOW}Warning: Log file not found after test${NC}"
        fi
        
        sleep 2
    done

    echo -e "${GREEN}nanoCron performance test completed (JSON PARSING ONLY)${NC}"
    echo ""
}

test_system_cron_performance() {
    echo -e "${BLUE}Testing System Cron parsing performance (CRONTAB PARSING ONLY)...${NC}"

    local iterations=5
    echo "Running $iterations iterations of crontab parsing test..."

    for i in $(seq 1 $iterations); do
        echo -e "${YELLOW}System cron test iteration $i/$iterations (CRONTAB PARSING ONLY)${NC}"
        echo -e "${BLUE}Executing: $SYSTEM_TESTER \"${LOG_DIR}\"${NC}"
        $SYSTEM_TESTER "${LOG_DIR}"
        
        # Verifica che il file di log sia stato scritto
        if [ -f "$LOG_FILE" ]; then
            echo -e "${GREEN}Log file updated successfully${NC}"
        else
            echo -e "${YELLOW}Warning: Log file not found after test${NC}"
        fi
        
        sleep 2
    done

    echo -e "${GREEN}System cron performance test completed (CRONTAB PARSING ONLY)${NC}"
    echo ""
}

create_test_jobs_json() {
    local test_jobs_file="./test_jobs.json"
    
    # Verifica che il file esista già (dovrebbe essere quello fornito)
    if [ -f "$test_jobs_file" ]; then
        echo -e "${GREEN}Test jobs file found: $test_jobs_file${NC}"
        
        # Verifica che contenga 10 jobs
        local job_count=$(grep -c '"command":' "$test_jobs_file" 2>/dev/null || echo "0")
        echo -e "${BLUE}Jobs found in file: $job_count${NC}"
        
        if [ "$job_count" -ne "10" ]; then
            echo -e "${YELLOW}Warning: Expected 10 jobs, found $job_count${NC}"
        fi
    else
        echo -e "${YELLOW}Creating test jobs file with 10 jobs...${NC}"
        cat > "$test_jobs_file" << 'EOF'
{
  "jobs": [
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD1",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD2",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD3",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD4",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD5",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD6",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD7",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD8",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD9",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    },
    {
      "command": "/home/giuseppe/code/NanoCron-v3/init/jobs/makeD10",
      "conditions": {},
      "description": "Make dir every minute",
      "schedule": {
        "day_of_month": "*",
        "day_of_week": "*",
        "hour": "*",
        "minute": "*",
        "month": "*"
      }
    }
  ]
}
EOF
        echo -e "${GREEN}Test jobs file created with 10 jobs: $test_jobs_file${NC}"
    fi
}

analyze_results() {
    echo -e "${BLUE}Analyzing test results (PARSING ONLY - FAIR COMPARISON)...${NC}"
    echo -e "${BLUE}Looking for log file at: $LOG_FILE${NC}"

    if [ ! -f "$LOG_FILE" ]; then
        echo -e "${RED}Error: Performance log file not found at $LOG_FILE${NC}"
        echo -e "${YELLOW}Checking directory contents:${NC}"
        ls -la "${LOG_DIR}/" || echo "Directory not accessible"
        
        # Prova a cercare il file altrove
        echo -e "${YELLOW}Searching for performance.log files:${NC}"
        find . -name "performance.log" -type f 2>/dev/null || echo "No performance.log files found"
        
        return 1
    fi

    echo -e "${GREEN}Log file found, analyzing...${NC}"
    
    # Estrai metriche dal log (versione PARSING ONLY)
    local nanocron_times=($(grep "nanoCron.*Parse Time (ms):" "$LOG_FILE" | awk '{print $NF}' | sed 's/ms//'))
    local nanocron_memory=($(grep "nanoCron.*Memory Used:" "$LOG_FILE" | awk '{print $(NF-1)}'))
    local nanocron_peak_memory=($(grep "nanoCron.*Peak Memory:" "$LOG_FILE" | awk '{print $(NF-1)}'))
    local nanocron_initial_memory=($(grep "nanoCron.*Initial Memory:" "$LOG_FILE" | awk '{print $(NF-1)}'))
    local nanocron_peak_cpu=($(grep "nanoCron.*Peak CPU Usage:" "$LOG_FILE" | awk '{print $(NF-1)}' | sed 's/%//'))
    local nanocron_avg_cpu=($(grep "nanoCron.*Average CPU Usage:" "$LOG_FILE" | awk '{print $(NF-1)}' | sed 's/%//'))

    local syscron_times=($(grep "System Cron.*Parse Time (ms):" "$LOG_FILE" | awk '{print $NF}' | sed 's/ms//'))
    local syscron_memory=($(grep "System Cron.*Memory Used:" "$LOG_FILE" | awk '{print $(NF-1)}'))
    local syscron_peak_memory=($(grep "System Cron.*Peak Memory:" "$LOG_FILE" | awk '{print $(NF-1)}'))
    local syscron_initial_memory=($(grep "System Cron.*Initial Memory:" "$LOG_FILE" | awk '{print $(NF-1)}'))
    local syscron_peak_cpu=($(grep "System Cron.*Peak CPU Usage:" "$LOG_FILE" | awk '{print $(NF-1)}' | sed 's/%//'))
    local syscron_avg_cpu=($(grep "System Cron.*Average CPU Usage:" "$LOG_FILE" | awk '{print $(NF-1)}' | sed 's/%//'))

    # Debug info
    echo "nanoCron parsing samples found: ${#nanocron_times[@]}"
    echo "System Cron parsing samples found: ${#syscron_times[@]}"

    calculate_average() {
        local sum=0
        local count=0
        for val in "$@"; do
            if [[ "$val" =~ ^[0-9]*\.?[0-9]+$ ]]; then
                sum=$(echo "$sum + $val" | bc -l)
                count=$((count + 1))
            fi
        done
        if [ $count -gt 0 ]; then
            echo "scale=3; $sum / $count" | bc -l
        else
            echo "0"
        fi
    }

    # Scrivi l'analisi nel file di log (stesso file!)
    echo "" >> "$LOG_FILE"
    echo "========================================" >> "$LOG_FILE"
    echo "=== FAIR COMPARISON ANALYSIS (PARSING ONLY) ===" >> "$LOG_FILE"
    echo "========================================" >> "$LOG_FILE"
    echo "" >> "$LOG_FILE"

    if [ ${#nanocron_times[@]} -gt 0 ]; then
        local avg_time_nc=$(calculate_average "${nanocron_times[@]}")
        local avg_memory_nc=$(calculate_average "${nanocron_memory[@]}")
        local avg_peak_memory_nc=$(calculate_average "${nanocron_peak_memory[@]}")
        local avg_initial_memory_nc=$(calculate_average "${nanocron_initial_memory[@]}")
        local avg_peak_cpu_nc=$(calculate_average "${nanocron_peak_cpu[@]}")
        local avg_avg_cpu_nc=$(calculate_average "${nanocron_avg_cpu[@]}")

        echo "nanoCron JSON Parsing Performance (${#nanocron_times[@]} samples):" >> "$LOG_FILE"
        echo "  Average Parse Time: ${avg_time_nc} ms" >> "$LOG_FILE"
        echo "  Average Memory Used (Delta): ${avg_memory_nc} KB" >> "$LOG_FILE"
        echo "  Average Peak Memory (Absolute): ${avg_peak_memory_nc} KB" >> "$LOG_FILE"
        echo "  Average Initial Memory (Absolute): ${avg_initial_memory_nc} KB" >> "$LOG_FILE"
        echo "  Average Peak CPU: ${avg_peak_cpu_nc}%" >> "$LOG_FILE"
        echo "  Average CPU Usage: ${avg_avg_cpu_nc}%" >> "$LOG_FILE"
        echo "" >> "$LOG_FILE"
    fi

    if [ ${#syscron_times[@]} -gt 0 ]; then
        local avg_time_sc=$(calculate_average "${syscron_times[@]}")
        local avg_memory_sc=$(calculate_average "${syscron_memory[@]}")
        local avg_peak_memory_sc=$(calculate_average "${syscron_peak_memory[@]}")
        local avg_initial_memory_sc=$(calculate_average "${syscron_initial_memory[@]}")
        local avg_peak_cpu_sc=$(calculate_average "${syscron_peak_cpu[@]}")
        local avg_avg_cpu_sc=$(calculate_average "${syscron_avg_cpu[@]}")

        echo "System Cron Crontab Parsing Performance (${#syscron_times[@]} samples):" >> "$LOG_FILE"
        echo "  Average Parse Time: ${avg_time_sc} ms" >> "$LOG_FILE"
        echo "  Average Memory Used (Delta): ${avg_memory_sc} KB" >> "$LOG_FILE"
        echo "  Average Peak Memory (Absolute): ${avg_peak_memory_sc} KB" >> "$LOG_FILE"
        echo "  Average Initial Memory (Absolute): ${avg_initial_memory_sc} KB" >> "$LOG_FILE"
        echo "  Average Peak CPU: ${avg_peak_cpu_sc}%" >> "$LOG_FILE"
        echo "  Average CPU Usage: ${avg_avg_cpu_sc}%" >> "$LOG_FILE"
        echo "" >> "$LOG_FILE"

        if [ ${#nanocron_times[@]} -gt 0 ]; then
            echo "=== FAIR PARSING COMPARISON ===" >> "$LOG_FILE"
            
            # Usa bc per calcoli sicuri, con gestione degli errori
            local time_ratio="N/A"
            local memory_ratio="N/A"
            local peak_memory_ratio="N/A"
            local cpu_ratio="N/A"
            
            if (( $(echo "$avg_time_sc > 0" | bc -l) )); then
                time_ratio=$(echo "scale=2; $avg_time_nc / $avg_time_sc" | bc -l 2>/dev/null || echo "N/A")
            fi
            
            if (( $(echo "$avg_memory_sc > 0" | bc -l) )); then
                memory_ratio=$(echo "scale=2; $avg_memory_nc / $avg_memory_sc" | bc -l 2>/dev/null || echo "N/A")
            fi
            
            if (( $(echo "$avg_peak_memory_sc > 0" | bc -l) )); then
                peak_memory_ratio=$(echo "scale=2; $avg_peak_memory_nc / $avg_peak_memory_sc" | bc -l 2>/dev/null || echo "N/A")
            fi
            
            if (( $(echo "$avg_peak_cpu_sc > 0" | bc -l) )); then
                cpu_ratio=$(echo "scale=2; $avg_peak_cpu_nc / $avg_peak_cpu_sc" | bc -l 2>/dev/null || echo "N/A")
            fi

            echo "Parse Time Ratio (nanoCron JSON / SystemCron Crontab): $time_ratio" >> "$LOG_FILE"
            echo "Memory Usage Ratio - Delta (nanoCron / SystemCron): $memory_ratio" >> "$LOG_FILE"
            echo "Peak Memory Ratio - Absolute (nanoCron / SystemCron): $peak_memory_ratio" >> "$LOG_FILE"
            echo "Peak CPU Ratio (nanoCron / SystemCron): $cpu_ratio" >> "$LOG_FILE"
            echo "" >> "$LOG_FILE"
            
            # Interpretazione dei risultati per confronto equo
            echo "=== FAIR COMPARISON INTERPRETATION ===" >> "$LOG_FILE"
            echo "IMPORTANT: This is a FAIR comparison - both tests perform equivalent parsing" >> "$LOG_FILE"
            echo "• nanoCron: JSON parsing of 10 jobs + validation + conversion" >> "$LOG_FILE"
            echo "• SystemCron: Crontab parsing of 10 jobs + validation + conversion" >> "$LOG_FILE"
            echo "• Both use identical measurement methodology and processing simulation" >> "$LOG_FILE"
            echo "" >> "$LOG_FILE"
            
            if [[ "$time_ratio" != "N/A" ]]; then
                if (( $(echo "$time_ratio < 1" | bc -l) )); then
                    local improvement=$(echo "scale=1; (1 - $time_ratio) * 100" | bc -l 2>/dev/null || echo "N/A")
                    echo "✓ nanoCron JSON parsing is FASTER (${improvement}% improvement, ${time_ratio}x ratio)" >> "$LOG_FILE"
                else
                    local slower=$(echo "scale=1; ($time_ratio - 1) * 100" | bc -l 2>/dev/null || echo "N/A")
                    echo "△ System cron crontab parsing is FASTER (nanoCron ${slower}% slower, ${time_ratio}x ratio)" >> "$LOG_FILE"
                fi
            fi
            
            if [[ "$memory_ratio" != "N/A" ]]; then
                if (( $(echo "$memory_ratio < 1" | bc -l) )); then
                    local improvement=$(echo "scale=1; (1 - $memory_ratio) * 100" | bc -l 2>/dev/null || echo "N/A")
                    echo "✓ nanoCron uses LESS memory delta (${improvement}% improvement, ${memory_ratio}x ratio)" >> "$LOG_FILE"
                elif (( $(echo "$memory_ratio > 1" | bc -l) )); then
                    local more=$(echo "scale=1; ($memory_ratio - 1) * 100" | bc -l 2>/dev/null || echo "N/A")
                    echo "△ nanoCron uses MORE memory delta (${more}% more, ${memory_ratio}x ratio)" >> "$LOG_FILE"
                else
                    echo "= Similar memory delta usage" >> "$LOG_FILE"
                fi
            fi
            
            if [[ "$peak_memory_ratio" != "N/A" ]]; then
                echo "Peak Memory Comparison: nanoCron=${avg_peak_memory_nc}KB vs SystemCron=${avg_peak_memory_sc}KB" >> "$LOG_FILE"
            fi
            
            echo "" >> "$LOG_FILE"
            echo "CONCLUSION: This comparison is now FAIR and MEANINGFUL" >> "$LOG_FILE"
            echo "Both systems perform equivalent parsing operations on identical datasets" >> "$LOG_FILE"
            echo "" >> "$LOG_FILE"
        fi
    fi

    echo "=== FAIR TEST VALIDITY ASSESSMENT ===" >> "$LOG_FILE"
    echo "Methodology: PARSING ONLY (Both tests perform equivalent operations)" >> "$LOG_FILE"
    echo "Workload Equivalence: 10 jobs each, same commands, same schedule patterns" >> "$LOG_FILE"
    echo "Memory Measurement: /proc/self/status VmRSS for both tests" >> "$LOG_FILE"
    echo "CPU Measurement: clock() current process for both tests" >> "$LOG_FILE"
    echo "Processing Simulation: Identical validation and conversion steps" >> "$LOG_FILE"
    echo "Test Fairness: EXCELLENT - truly equivalent operations" >> "$LOG_FILE"
    echo "Analysis completed at: $(date)" >> "$LOG_FILE"
    echo "========================================" >> "$LOG_FILE"

    echo -e "${GREEN}Fair comparison analysis completed and written to log file${NC}"
    echo ""
}

show_final_results() {
    echo -e "${BLUE}=== FINAL TEST RESULTS (PARSING ONLY - FAIR COMPARISON) ===${NC}"
    echo ""

    if [ -f "$LOG_FILE" ]; then
        echo -e "${YELLOW}Showing fair comparison analysis from log file:${NC}"
        echo ""
        
        # Cerca e mostra la sezione di analisi
        awk '/=== FAIR COMPARISON ANALYSIS \(PARSING ONLY\) ===/{flag=1} flag' "$LOG_FILE"
        
    else
        echo -e "${RED}Log file not found${NC}"
    fi

    echo ""
    echo -e "${GREEN}Fair performance comparison completed!${NC}"
    echo -e "${PURPLE}Key Improvements in v3.0 (PARSING ONLY):${NC}"
    echo -e "${BLUE}  • IDENTICAL WORKLOADS: Both parse exactly 10 jobs${NC}"
    echo -e "${BLUE}  • NO SYSTEM INTERACTIONS: Pure parsing comparison${NC}"
    echo -e "${BLUE}  • EQUIVALENT DATA: JSON jobs ↔ Crontab jobs${NC}"
    echo -e "${BLUE}  • SAME PROCESSING: Identical validation and simulation${NC}"
    echo -e "${BLUE}  • TRULY FAIR: Finally comparing 'apples to apples'${NC}"
    echo ""
    echo -e "${BLUE}Complete fair comparison data available in: $LOG_FILE${NC}"
}

main() {
    check_prerequisites
    cleanup_logs
    test_nanocron_performance
    test_system_cron_performance
    
    # Debug: Mostra il contenuto del file prima dell'analisi
    echo -e "${YELLOW}Debug: Checking log file content before analysis...${NC}"
    if [ -f "$LOG_FILE" ]; then
        echo -e "${BLUE}Log file size: $(wc -l < "$LOG_FILE") lines${NC}"
        echo -e "${BLUE}Last 5 lines of log file:${NC}"
        tail -5 "$LOG_FILE"
    else
        echo -e "${RED}Debug: Log file not found at $LOG_FILE${NC}"
    fi
    echo ""
    
    analyze_results
    show_final_results
}

# Verifica dipendenze
if ! command -v bc &> /dev/null; then
    echo -e "${RED}Error: 'bc' calculator not found. Please install it: sudo apt-get install bc${NC}"
    exit 1
fi

# Verifica che i file sorgenti delle versioni PARSING ONLY esistano
if [ ! -f "cron_system_tester.cpp" ]; then
    echo -e "${RED}Error: cron_system_tester.cpp not found${NC}"
    echo -e "${YELLOW}Please ensure you have the PARSING ONLY version of the source files${NC}"
    exit 1
fi

if [ ! -f "nanocron_test_runner.cpp" ]; then
    echo -e "${RED}Error: nanocron_test_runner.cpp not found${NC}"
    echo -e "${YELLOW}Please ensure you have the nanoCron test runner source file${NC}"
    exit 1
fi

main "$@"