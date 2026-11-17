// Exercise the LAMZU driver with a simulated feature-report transport; no hardware needed.
#include "../src/drivers/drivers.h"

#include <cassert>
#include <cstdio>

namespace {
std::vector<BYTE> lastCmd, reply;

HidColl Collection(USHORT vid, USHORT pid, USHORT page = 0xFFFF, USHORT featLen = 65) {
    HidColl c;
    c.path = L"lamzu";
    c.vid = vid;
    c.pid = pid;
    c.caps.UsagePage = page;
    c.caps.FeatureReportByteLength = featLen;
    return c;
}

// Reply captured from a LAMZU INCA 8K receiver (63 %, not charging), with the leading report ID.
std::vector<BYTE> Reply(BYTE charging, BYTE percent) {
    std::vector<BYTE> r(65, 0);
    r[1] = 0xA1; r[3] = 0x02; r[4] = 0x02; r[6] = 0x83; r[7] = charging; r[8] = percent;
    return r;
}
}  // namespace

bool HidDev::OpenFeatureOnly(const HidColl& c) {
    c_ = c;
    h_ = reinterpret_cast<HANDLE>(1);
    return true;
}
void HidDev::Close() { h_ = INVALID_HANDLE_VALUE; }
bool HidDev::SetFeature(const std::vector<BYTE>& data) { lastCmd = data; return true; }
bool HidDev::GetFeature(BYTE, std::vector<BYTE>& out) { out = reply; return !reply.empty(); }
std::wstring HidProduct(const HidColl&) { return L"LAMZU INCA 8K Receiver"; }
std::wstring Hex4(USHORT v) {
    wchar_t text[8];
    swprintf(text, 8, L"%04X", v);
    return text;
}
int Clamp100(int v) { return v < 0 ? 0 : v > 100 ? 100 : v; }

int main() {
    PollContext ctx;
    ctx.colls = { Collection(0x37B0, 0x0010) };
    reply = Reply(0, 63);
    PollLamzu(ctx);
    assert(lastCmd.size() == 65 && lastCmd[0] == 0 && lastCmd[3] == 2 && lastCmd[4] == 2 && lastCmd[6] == 0x83);
    assert(ctx.out.size() == 1);
    assert(ctx.out[0].key == L"lamzu:37B0:0010" && ctx.out[0].name == L"LAMZU INCA 8K");
    assert(ctx.out[0].percent == 63 && !ctx.out[0].charging && ctx.out[0].via == L"2.4 GHz receiver");

    // Cable, charging.
    ctx = PollContext();
    ctx.colls = { Collection(0x37B0, 0x0009) };
    reply = Reply(1, 80);
    PollLamzu(ctx);
    assert(ctx.out.size() == 1 && ctx.out[0].charging && ctx.out[0].percent == 80 && ctx.out[0].via == L"USB cable");

    // Receiver with the mouse off.
    ctx = PollContext();
    ctx.colls = { Collection(0x373E, 0x001E) };
    reply = Reply(0, 0);
    PollLamzu(ctx);
    assert(ctx.out.size() == 1 && !ctx.out[0].connected && ctx.out[0].percent == -1);

    // No ack: nothing reported, nothing claimed.
    ctx = PollContext();
    ctx.colls = { Collection(0x37B0, 0x0010) };
    reply = std::vector<BYTE>(65, 0);
    PollLamzu(ctx);
    assert(ctx.out.empty() && !ctx.IsClaimed(0x37B0, 0x0010));

    // Other vendors and other collections are never written to.
    ctx = PollContext();
    lastCmd.clear();
    ctx.colls = { Collection(0x3554, 0xF50D), Collection(0x37B0, 0x0010, 0xFFA0), Collection(0x37B0, 0x0010, 0xFFFF, 0) };
    reply = Reply(0, 50);
    PollLamzu(ctx);
    assert(ctx.out.empty() && lastCmd.empty());

    std::puts("lamzu driver test: ok");
    return 0;
}
