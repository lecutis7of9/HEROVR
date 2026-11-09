"""Read-only Windows Application Error (Event 1000) crash evidence.

Used only after a failed startup; never changes the game, Windows settings,
or the crash-reporting configuration. A missing/unavailable event is normal.
"""
import datetime
import subprocess
import xml.etree.ElementTree as ET

FIELDS = ("AppName", "AppVersion", "ModuleName", "ModuleVersion",
          "ExceptionCode", "FaultingOffset", "ProcessId")


def parse_events(xml, pid=None, after=None):
    """Return concise Spider-Man 2 faults from native Windows event XML."""
    try:
        root = ET.fromstring("<Events>" + xml + "</Events>")
    except ET.ParseError:
        return []
    out = []
    for event in root:
        system = event.find("{*}System")
        data = event.find("{*}EventData")
        if system is None or data is None:
            continue
        event_id = system.find("{*}EventID")
        if event_id is None or event_id.text != "1000":
            continue
        fields = {}
        for entry in data.findall("{*}Data"):
            name = entry.get("Name")
            if name in FIELDS:
                fields[name] = (entry.text or "").strip()
        if fields.get("AppName", "").lower() != "spider-man2.exe":
            continue
        if pid is not None:
            try:
                event_pid = int(fields.get("ProcessId", ""), 0)
            except ValueError:
                continue
            if event_pid != pid:
                continue
        stamp = system.find("{*}TimeCreated")
        stamp = stamp.get("SystemTime") if stamp is not None else None
        if after is not None and stamp is not None:
            try:
                dt = datetime.datetime.fromisoformat(stamp.replace("Z", "+00:00"))
                if dt < after:
                    continue
            except ValueError:
                continue
        out.append({"time_utc": stamp, **fields})
    return out


def recent_game_faults(pid=None, minutes=10):
    """Use built-in wevtutil; no elevated access required for Application log."""
    try:
        result = subprocess.run(
            ["wevtutil", "qe", "Application",
             "/q:*[System[(EventID=1000)]]", "/rd:true", "/c:40", "/f:xml"],
            capture_output=True, text=True, encoding="utf-8",
            errors="replace", timeout=4, check=False)
        if result.returncode:
            return []
        after = datetime.datetime.now(datetime.timezone.utc) - datetime.timedelta(minutes=minutes)
        return parse_events(result.stdout.lstrip("\ufeff"), pid=pid, after=after)[:4]
    except (OSError, subprocess.TimeoutExpired):
        return []
