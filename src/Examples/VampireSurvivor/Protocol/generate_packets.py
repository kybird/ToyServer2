
import os
import re

def generate_game_packets():
    proto_path = 'game.proto'
    output_path = '../Common/GamePackets.h'
    
    if not os.path.exists(proto_path):
        print(f"Error: {proto_path} not found")
        return

    with open(proto_path, 'r', encoding='utf-8') as f:
        content = f.read()

    msg_ids = {}
    enum_match = re.search(r'enum\s+MsgId\s*\{([^\}]*)\}', content, re.DOTALL)
    if enum_match:
        enum_body = enum_match.group(1)
        for line in enum_body.split('\n'):
            line = re.sub(r'//.*', '', line).strip()
            id_match = re.match(r'(\w+)\s*=\s*(\d+)', line)
            if id_match:
                msg_ids[id_match.group(1)] = id_match.group(2)

    valid_packets = []
    for msg_id_name in msg_ids.keys():
        if msg_id_name == "NONE": continue
        
        # Try different mappings to find the message name in .proto
        # 1. Direct match (e.g. message S_LOGIN)
        # 2. S_LOGIN -> S_Login
        # 3. S_SKILL_EFFECT -> S_SkillEffect
        
        parts = msg_id_name.split('_')
        
        # Option 2 & 3: S_Login or S_SkillEffect
        if len(parts) >= 2:
            prefix = parts[0].capitalize() if parts[0].lower() in ['s', 'c'] else parts[0].capitalize()
            # If parts[0] is 'S' or 'C', keep it as is but capitalize (which is already done)
            # Actually, proto has S_Login (S is cap).
            prefix = parts[0] 
            body = "".join(p.capitalize() for p in parts[1:])
            pascal_name = f"{prefix}_{body}"
        else:
            pascal_name = msg_id_name.capitalize()

        if f"message {msg_id_name}" in content:
            valid_packets.append((msg_id_name, msg_id_name, msg_id_name))
        elif f"message {pascal_name}" in content:
            valid_packets.append((msg_id_name, pascal_name, pascal_name))
        else:
            # Try one more: S_SKILL_EFFECT -> S_Skill_Effect
            alt_pascal = "_".join(p.capitalize() for p in parts)
            if f"message {alt_pascal}" in content:
                valid_packets.append((msg_id_name, alt_pascal, alt_pascal))

    lines = [
        '#pragma once',
        '',
        '#include "Protocol.h"',
        '#include "Protocol/game.pb.h"',
        '#include "System/Packet.h"',
        '',
        'namespace SimpleGame {',
        '',
        '// Generic Template for Protobuf Packets',
        'template <uint16_t PacketId, typename TProto>',
        'class ProtobufPacket : public System::ProtobufPacketBase<ProtobufPacket<PacketId, TProto>, PacketHeader, TProto>',
        '{',
        'public:',
        '    static constexpr uint16_t ID = PacketId;',
        '    using System::ProtobufPacketBase<ProtobufPacket<PacketId, TProto>, PacketHeader, TProto>::ProtobufPacketBase;',
        '};',
        ''
    ]

    for msg_id, proto_name, class_prefix in valid_packets:
        lines.append(f'using {class_prefix}Packet = ProtobufPacket<PacketID::{msg_id}, Protocol::{proto_name}>;')

    lines.append('')
    lines.append('} // namespace SimpleGame')
    
    with open(output_path, 'w', encoding='utf-8') as f:
        f.write('\n'.join(lines))
        f.write('\n')
    
    print(f"Successfully generated {output_path} with templates")

if __name__ == "__main__":
    generate_game_packets()
