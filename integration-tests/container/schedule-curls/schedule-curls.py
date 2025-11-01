import sys
import time
import subprocess

if len(sys.argv) != 6:
    print("Usage: python fixed_interval_curl.py NUM_META_ITER NUM_ITER FIXED_INTERVAL_SEC SLEEP_BETWEEN_META_ITER URL", file=sys.stderr)
    sys.exit(1)


try:
    num_meta_iter = int(sys.argv[1])
    num_iter = int(sys.argv[2])
    fixed_interval = float(sys.argv[3])
    sleep_between_iterations = float(sys.argv[4])
    url = sys.argv[5]
except ValueError as e:
    print(f"Error parsing numerical arguments: {e}", file=sys.stderr)
    sys.exit(1)


def run_curl(target_url: str):
    try:
        subprocess.run(['curl', target_url], check=True)
    except subprocess.CalledProcessError as e:
        print(f"Curl failed with exit code {e.returncode}: {e}", file=sys.stderr)
        sys.exit(e.returncode)
    except FileNotFoundError:
        print("Error: 'curl' command not found. Ensure it is installed and in your PATH.", file=sys.stderr)
        sys.exit(1)


i = 0
while i < num_meta_iter:
    j = 0
    next_execution_time = time.time()
    while j < num_iter:
        current_time = time.time()
        sleep_duration = next_execution_time - current_time
        if sleep_duration > 0:
            time.sleep(sleep_duration)
        else:
            pass
        execution_time = time.time()
        time_str = time.strftime("%H:%M:%S", time.localtime(execution_time))
        milliseconds = f"{execution_time % 1:.3f}"[2:]
        print(f"[{time_str}.{milliseconds}] Executing curl (Meta: {i + 1}/{num_meta_iter}, Iter: {j + 1}/{num_iter})")
        run_curl(url)
        next_execution_time += fixed_interval
        j += 1
    if sleep_between_iterations > 0:
        time.sleep(sleep_between_iterations)
    i += 1
print("Script finished successfully.")
