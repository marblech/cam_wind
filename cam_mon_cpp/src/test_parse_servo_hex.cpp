#include "protocol.h"
#include <iostream>
#include <vector>
#include <iomanip>
#include <cstring>

using namespace std;
using namespace cammon;

static void dump_hex(const vector<uint8_t>& v, size_t start=0, size_t len=0) {
    if (len==0 || start+len>v.size()) len = v.size()-start;
    for (size_t i=0;i<len;++i) {
        if (i%16==0) cout << endl << setw(4) << setfill('0') << hex << (start+i) << ": ";
        cout << setw(2) << setfill('0') << hex << (int)v[start+i] << " ";
    }
    cout << dec << setfill(' ') << endl;
}

int main() {
    // Raw hex bytes as provided (contains outer frame 0x0F 0xF0 ... 0xF0 0x0F and inner 0x7E servo frame)
    vector<uint8_t> raw = {
        0x0F,0xF0,0x05,0x00,0x00,
        0x7E,0x48,0x00,0x48,0x02,0x01,0x01,0x09,0x00,
        0x00,0x00,0x70,0x42,0x00,0x00,0x70,0x42,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,
        0x00,0x00,0x00,0x71,0xF5,0xF0,0x0F
    };

    cout << "[test_parse_servo_hex] raw length = " << raw.size() << " bytes\n";
    dump_hex(raw);

    // find first 0x7E header
    size_t pos7e = SIZE_MAX;
    for (size_t i=0;i<raw.size();++i) if (raw[i]==0x7E) { pos7e = i; break; }

    if (pos7e==SIZE_MAX) {
        cerr << "No 0x7E header found\n";
        return 1;
    }
    cout << "\nFound 0x7E at index " << pos7e << "\n";

    // if we have at least 72 bytes from pos7e, try deserialize_servo
    if (pos7e + 72 <= raw.size()) {
        vector<uint8_t> frame(raw.begin()+pos7e, raw.begin()+pos7e+72);
        auto sp = ServoPacket::deserialize_servo(frame);
        if (sp.has_value()) {
            ServoPacket s = sp.value();
            cout << "ServoPacket parsed successfully via deserialize_servo()\n";
            cout << " header=" << hex << int(s.header) << dec
                 << " frame_len=" << int(s.frame_len)
                 << " seq=" << int(s.seq)
                 << " device_type=" << int(s.device_type)
                 << " packet_type=" << int(s.packet_type)
                 << " device_ip=" << int(s.device_ip)
                 << " main_conn=" << int(s.main_conn)
                 << " control=0x" << hex << int(s.control) << dec << "\n";
            cout << fixed << setprecision(6);
            cout << " azimuth=" << s.azimuth << " deg\n";
            cout << " elevation=" << s.elevation << " deg\n";
            cout << " az_speed=" << s.az_speed << " deg/s\n";
            cout << " el_speed=" << s.el_speed << " deg/s\n";
            cout << " target_distance=" << s.target_distance << "\n";
            cout << " checksum(byte)=" << hex << int(s.checksum) << dec << "\n";
        } else {
            cerr << "deserialize_servo() returned nullopt (checksum or length mismatch)\n";
        }
    } else {
        cout << "Not enough bytes for full 72-byte servo frame from 0x7E position (have "
             << (raw.size()-pos7e) << " bytes). Will attempt manual decode of key fields.\n";

        // manual decode of fields accessible in provided bytes
        size_t base = pos7e;
        if (base + 12 + 4 <= raw.size()) {
            // azimuth at base+9..base+12 (LE float)
            size_t az_idx = base + 9;
            if (az_idx + 4 <= raw.size()) {
                uint32_t azv = (uint32_t)raw[az_idx] | ((uint32_t)raw[az_idx+1]<<8) | ((uint32_t)raw[az_idx+2]<<16) | ((uint32_t)raw[az_idx+3]<<24);
                float az_f; memcpy(&az_f, &azv, 4);
                cout << "Manual decode: azimuth = " << az_f << " deg\n";
            }
            size_t el_idx = base + 13;
            if (el_idx + 4 <= raw.size()) {
                uint32_t elv = (uint32_t)raw[el_idx] | ((uint32_t)raw[el_idx+1]<<8) | ((uint32_t)raw[el_idx+2]<<16) | ((uint32_t)raw[el_idx+3]<<24);
                float el_f; memcpy(&el_f, &elv, 4);
                cout << "Manual decode: elevation = " << el_f << " deg\n";
            }
        } else {
            cerr << "Insufficient bytes to read az/el floats\n";
        }

        // compute XOR checksum for bytes 0..70 if those bytes exist in the buffer (relative to servo frame start)
        size_t available = raw.size() - base;
        size_t need = 72; // full frame length
        size_t have_for_cs = min<size_t>(available, 71); // need bytes 0..70
        if (have_for_cs >= 1) {
            uint8_t cs = 0;
            for (size_t i=0;i<have_for_cs;++i) cs ^= raw[base + i];
            cout << "Computed XOR of available bytes (frame[0]..frame[" << have_for_cs-1 << "]) = 0x" << hex << int(cs) << dec << "\n";
            if (base + 71 < raw.size()) {
                cout << "Provided checksum byte at frame[71] = 0x" << hex << int(raw[base+71]) << dec << "\n";
            } else {
                cout << "No provided checksum byte available in input\n";
            }
        } else {
            cerr << "No bytes available to compute checksum\n";
        }
    }

    return 0;
}