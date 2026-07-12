import re
import sys
from collections import defaultdict, Counter

def parse_log(path):
    events = []
    pattern = re.compile(r"\[(\d+)\] pid=(\d+) syscall=(\d+) arg=(\d+)")
    with open(path) as f:
        for line in f:
            m = pattern.match(line)
            if m:
                tick, pid, sc, arg = map(int, m.groups())
                events.append((tick, pid, sc, arg))
    return events

def sequences_by_pid(events):
    seqs = defaultdict(list)
    for _, pid, sc, _ in events:
        seqs[pid].append(sc)
    return seqs

def build_ngram_model(seqs, n=2):
    counts = Counter()
    for seq in seqs.values():
        for i in range(len(seq) - n + 1):
            counts[tuple(seq[i:i+n])] += 1
    return counts

def score_sequence(seq, model, n=2):
    total, unseen = 0, 0
    for i in range(len(seq) - n + 1):
        gram = tuple(seq[i:i+n])
        total += 1
        if model[gram] <= 1:  # only appears in this sequence itself
            unseen += 1
    return unseen / total if total else 0

def main():
    if len(sys.argv) < 2:
        print("Usage: python3 analyze.py <syscalls.log> [baseline_pid1,baseline_pid2,...]")
        sys.exit(1)

    events = parse_log(sys.argv[1])
    seqs = sequences_by_pid(events)

    if not seqs:
        print("No syscall events found in log.")
        sys.exit(1)

    if len(sys.argv) >= 3:
        baseline_pids = set(int(p) for p in sys.argv[2].split(","))
    else:
        # default: treat all but the last pid as baseline
        all_pids = sorted(seqs.keys())
        baseline_pids = set(all_pids[:-1])

    baseline_seqs = {pid: seq for pid, seq in seqs.items() if pid in baseline_pids}
    model = build_ngram_model(baseline_seqs, n=2)

    print(f"Baseline built from PIDs: {sorted(baseline_pids)}\n")
    print(f"{'PID':<6}{'Length':<8}{'Score':<8}Status")
    print("-" * 35)
    for pid, seq in sorted(seqs.items()):
        score = score_sequence(seq, model, n=2)
        status = "ANOMALOUS" if score > 0.5 else "normal"
        print(f"{pid:<6}{len(seq):<8}{score:<8.2f}{status}")

if __name__ == "__main__":
    main()
