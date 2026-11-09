"""Offline tests for automatic Windows fault report parsing."""
import datetime
import unittest
from windows_fault import parse_events

EVENT = """<Event xmlns="http://schemas.microsoft.com/win/2004/08/events/event">
<System><EventID>1000</EventID><TimeCreated SystemTime="2026-11-09T09:05:00.0000000Z"/></System>
<EventData>
<Data Name="AppName">Spider-Man2.exe</Data>
<Data Name="ModuleName">spidy_render_probe.dll</Data>
<Data Name="ExceptionCode">c0000409</Data>
<Data Name="ProcessId">0x2938</Data>
</EventData></Event>"""


class WindowsFaultTests(unittest.TestCase):
    def test_matching_pid_and_time(self):
        since = datetime.datetime(2026, 11, 9, 9, 0, tzinfo=datetime.timezone.utc)
        result = parse_events(EVENT, pid=0x2938, after=since)
        self.assertEqual(result[0]["ModuleName"], "spidy_render_probe.dll")
        self.assertEqual(result[0]["ExceptionCode"], "c0000409")

    def test_excludes_other_process_or_stale_events(self):
        self.assertEqual(parse_events(EVENT, pid=1234), [])
        after = datetime.datetime(2026, 11, 9, 9, 6, tzinfo=datetime.timezone.utc)
        self.assertEqual(parse_events(EVENT, after=after), [])

    def test_invalid_xml(self):
        self.assertEqual(parse_events("not xml"), [])


if __name__ == "__main__":
    unittest.main()
