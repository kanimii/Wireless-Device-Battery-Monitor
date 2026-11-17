// LAMZU mice (VIDs 37B0 and 373E: Maya, Maya X, Inca, Thorn, Paro, Tachi, Atlantis OG / Mini, Orcus).
// Protocol from the official web hub (lamzu.net, "LAMZU Aurora" v1.0.38, getBatPer), confirmed on
// an INCA 8K receiver (37B0:0010). The battery query goes on the vendor collection FFFF:0000 as
// unnumbered 64-byte feature reports:
//   request [00][..] [02 02 00 83]   (payload bytes 2, 3 and 5)
//   reply   [00][A1 .. 02 .. 83][charging][percent]   (A1 = ack, echo of the command)
// LAMZU Atlantis on VID 3554 uses a different (Compx) protocol and is not handled here.
#include "drivers.h"

#include <algorithm>

namespace {

// Wired PIDs from the hub's env-models.json; every other PID on these VIDs is a receiver.
bool IsCable(USHORT vid, USHORT pid) {
    static const UINT kCable[] = {
        0x373E001C, 0x37B00005, 0x37B00007, 0x37B00009, 0x37B00011, 0x37B00017, 0x37B0001C, 0x37B0001F,
        0x37B00021, 0x37B00025, 0x37B00028, 0x37B0002C, 0x37B00030, 0x37B00034, 0x37B00038, 0x37B0003A,
        0x37B0003C, 0x37B0003E, 0x37B00040, 0x37B00042, 0x37B00044, 0x37B00046, 0x37B00050, 0x37B00052,
        0x37B00056,
    };
    UINT key = ((UINT)vid << 16) | pid;
    return std::find(std::begin(kCable), std::end(kCable), key) != std::end(kCable);
}

bool QueryBattery(HidDev& d, std::vector<BYTE>& r) {
    for (int attempt = 0; attempt < 3; attempt++) {
        std::vector<BYTE> cmd(d.coll().featLen(), 0);
        cmd[3] = 0x02;
        cmd[4] = 0x02;
        cmd[6] = 0x83;
        if (!d.SetFeature(cmd)) return false;
        Sleep(100);  // the hub waits 100 ms before reading the reply
        if (d.GetFeature(0, r) && r.size() >= 9 && r[1] == 0xA1 && r[4] == 0x02 && r[6] == 0x83) return true;
    }
    return false;
}

}  // namespace

void PollLamzu(PollContext& ctx) {
    for (USHORT vendor : { (USHORT)0x37B0, (USHORT)0x373E }) {
        for (auto [vid, pid] : ctx.Products(vendor)) {
            for (const HidColl* c : ctx.Find(vid, pid, [](const HidColl& c) { return c.page() == 0xFFFF && c.featLen() == 65; })) {
                HidDev d;
                if (!d.OpenFeatureOnly(*c)) continue;
                std::vector<BYTE> r;
                if (!QueryBattery(d, r)) continue;
                bool cable = IsCable(vid, pid);
                DeviceStatus s;
                s.key = L"lamzu:" + Hex4(vid) + L":" + Hex4(pid);
                s.name = HidProduct(*c);
                size_t at = s.name.find(L" Receiver");
                if (at != std::wstring::npos) s.name.erase(at);
                if (s.name.empty()) s.name = L"LAMZU mouse";
                s.kind = DeviceKind::Mouse;
                s.via = cable ? L"USB cable" : L"2.4 GHz receiver";
                s.charging = r[7] == 1;
                // shortcut: 0 % on the receiver is read as "mouse off" (not verified on hardware); revisit with a capture.
                if (r[8] == 0 && !cable) s.connected = false;
                else s.percent = Clamp100(r[8]);
                ctx.Add(s, vid, pid);
                break;
            }
        }
    }
}
