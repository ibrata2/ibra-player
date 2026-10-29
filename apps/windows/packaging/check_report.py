"""Summarise the self-test report and fail the build if a real format did not play."""
import json
import sys

report = json.load(open(sys.argv[1], encoding="utf-8"))
failed = []
rows = []
for r in report:
    info = r.get("info", {})
    name = r["file"]
    rows.append("| {} | {} | {} | {:.1f}s | {} | {} | {} | {} |".format(
        name, "yes" if r.get("loaded") else "no", "yes" if r.get("playing") else "no",
        r.get("advancedSeconds", 0), info.get("videoCodec", "")[:28], info.get("audioCodecName", ""),
        info.get("hwdec", "") or "software", r.get("error", "") or info.get("error", "")))
    expected_fail = name.startswith("z_")
    if expected_fail:
        if r.get("loaded"):
            failed.append(name + " (corrupt file should have been rejected)")
        continue
    if not r.get("loaded") or not r.get("playing"):
        failed.append(name)

table = "\n".join(["| File | Loaded | Playing | Advanced | Video | Audio | Decoder | Error |",
                   "|---|---|---|---|---|---|---|---|"] + rows)
print(table)
with open(sys.argv[2], "w", encoding="utf-8") as f:
    f.write("## iBRA PLAYER self-test on Windows\n\n" + table + "\n")
if failed:
    print("\nFAILED:", ", ".join(failed))
    sys.exit(1)
print(f"\nAll {len(report)} files behaved as expected.")
