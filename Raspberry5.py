import requests
import json
import yaml
import serial
import time
from cbs.cbs import CBS, Environment

# Cấu hình UART
UART_PORT = "/dev/serial0"  # UART mặc định trên Raspberry Pi 5
UART_BAUDRATE = 115200

# Lấy dữ liệu bản đồ từ HTTPS
def fetch_map_data(url):
    try:
        response = requests.get(url, timeout=10)
        response.raise_for_status()
        return response.json()
    except requests.RequestException as e:
        print(f"Lỗi khi lấy dữ liệu bản đồ: {e}")
        return None

# Chuyển đổi JSON sang YAML cho CBS (giữ nguyên như yêu cầu)
def convert_json_to_yaml(json_data, yaml_file):
    try:
        # Tạo ánh xạ từ node ID sang tọa độ
        node_map = {node["id"]: [node["x"], node["y"]] for node in json_data["nodes"]}

        # Tạo danh sách node (dùng list thay vì tuple)
        width = json_data["dimensions"]["width"]
        height = json_data["dimensions"]["height"]
        nodes = [[x, y] for x in range(width) for y in range(height)]

        # Tạo danh sách edge từ JSON
        edges = [[edge["source"], edge["target"], 1] for edge in json_data["edges"]
                 if edge["source"] in node_map and edge["target"] in node_map]

        # Tạo YAML data
        yaml_data = {
            "map": {
                "dimension": [width, height],
                "nodes": nodes,
                "edges": edges,
                "obstacles": []
            },
            "agents": []
        }
        with open(yaml_file, 'w') as f:
            yaml.dump(yaml_data, f, default_flow_style=None)
        return yaml_data, node_map
    except Exception as e:
        print(f"Lỗi khi chuyển đổi JSON sang YAML: {e}")
        return None, None

# Tìm đường đi và tạo lịch trình
def plan_paths(start_ids, goal_ids, yaml_file, node_map):
    try:
        # Ánh xạ ID sang tọa độ
        start1 = node_map.get(start_ids[0], [4, 5])  # Mặc định hợp lý nếu thiếu ID
        goal1 = node_map.get(goal_ids[0], [8, 4])
        start2 = node_map.get(start_ids[1], [1, 11])
        goal2 = node_map.get(goal_ids[1], [16, 8])

        # Cập nhật file YAML với thông tin agents
        with open(yaml_file, 'r') as f:
            yaml_data = yaml.load(f, Loader=yaml.SafeLoader)

        # Sửa dimension thành dimensions cho tương thích với cbs.py
        if "dimension" in yaml_data["map"]:
            yaml_data["map"]["dimensions"] = yaml_data["map"].pop("dimension")

        yaml_data["agents"] = [
            {"start": start1, "goal": goal1, "name": "agv1"},
            {"start": start2, "goal": goal2, "name": "agv2"}
        ]
        with open(yaml_file, 'w') as f:
            yaml.safe_dump(yaml_data, f, default_flow_style=None)

        # Tạo môi trường CBS
        dimension = yaml_data["map"]["dimensions"]
        obstacles = yaml_data["map"]["obstacles"]
        agents = yaml_data["agents"]

        # Khởi tạo và chạy CBS
        env = Environment(dimension, agents, obstacles)
        cbs = CBS(env)
        solution = cbs.search()
        if not solution:
            print("Không tìm thấy giải pháp")
            return None, None

        # Tạo lịch trình từ giải pháp
        schedule = []
        max_length = max(len(path) for path in solution.values())
        for t in range(max_length):
            step = {}
            for agent, path in solution.items():
                if t < len(path):
                    step[agent] = [path[t]['x'], path[t]['y']]
                else:
                    step[agent] = [path[-1]['x'], path[-1]['y']]  # Giữ nguyên vị trí cuối
            schedule.append(step)

        return solution, schedule
    except Exception as e:
        print(f"Lỗi khi lập kế hoạch đường đi: {e}")
        return None, None

# Gửi tín hiệu qua UART
def send_signal(agv_id, command):
    try:
        with serial.Serial(UART_PORT, UART_BAUDRATE, timeout=1) as ser:
            message = f"{agv_id}:{command}\n"
            ser.write(message.encode('utf-8'))
            print(f"Gửi lệnh đến {agv_id}: {command}")
    except Exception as e:
        print(f"Lỗi khi gửi tín hiệu qua UART: {e}")

# Hàm chính
def main():
    # Lấy dữ liệu bản đồ
    map_url = "https://hackathon.omelet.tech/api/maps/df668aa9-bed0-4207-814a-4ec1ff478405/"
    map_data = fetch_map_data(map_url)
    if not map_data:
        return

    # Chuyển đổi sang YAML
    yaml_file = "input.yaml"
    yaml_data, node_map = convert_json_to_yaml(map_data, yaml_file)
    if not yaml_data or not node_map:
        return

    # Lập kế hoạch đường đi
    start_ids = map_data["startingPositions"]  # [27, 142]
    goal_ids = map_data["destinationPositions"]  # [45, 171]
    paths, schedule = plan_paths(start_ids, goal_ids, yaml_file, node_map)
    if not paths or not schedule:
        return

    # Gửi tín hiệu đến AGV qua UART
    for t, step in enumerate(schedule):
        print(f"Thời điểm {t}:")
        for agv_id, position in step.items():
            command = f"move_to:{position[0]},{position[1]}"
            send_signal(agv_id, command)
        time.sleep(1)  # Đợi để đồng bộ hóa lệnh

if __name__ == "__main__":
    main()