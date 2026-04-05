import serial
import time
import math

# --- 配置 ---
PORT = 'COM12' 
BAUD = 115200

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
except:
    print(f"无法打开串口 {PORT}"); exit()

def get_data():
    while True:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line.startswith("DATA:"):
            try:
                parts = line.replace("DATA:", "").split(',')
                # 返回：ax, ay, az, gx, gy, gz, mx, my, mz
                return [float(x) for x in parts]
            except: continue

def main():
    print("--- 传感器极性与轴向诊断程序 ---")
    print("请按照以下指令操作，并记录数值变化：\n")

    input("步骤 1: 准备好后按回车，保持传感器水平静止...")
    data = get_data()
    print(f"当前静止加速度 (应az接近1000): ax={data[0]}, ay={data[1]}, az={data[2]}")
    print(f"当前静止陀螺仪 (应接近0): gx={data[3]}, gy={data[4]}, gz={data[5]}")

    print("\n" + "="*50)
    print("步骤 2: 水平【顺时针】快速旋转传感器")
    print("观察 GZ 的正负号...")
    for _ in range(200):
        data = get_data()
        print(f"GZ角速度: {data[5]:.2f}", end='\r')
        time.sleep(0.05)
    
    print("\n" + "="*50)
    print("步骤 3: 保持水平，缓慢旋转，找到【磁北】方向")
    print("观察 mx_raw 和 my_raw 的变化...")
    print("提示：当 mx_raw 最大且 my_raw 接近 0 时，说明 X轴正对着北方。")
    try:
        while True:
            data = get_data()
            mx, my, mz = data[6], data[7], data[8]
            # 计算一个最基础的原始角度（不带补偿）
            raw_heading = math.degrees(math.atan2(my, mx))
            print(f"RAW_MAG: mx={mx:>6}, my={my:>6}, mz={mz:>6} | Heading: {raw_heading:>6.1f}°", end='\r')
            time.sleep(0.1)
    except KeyboardInterrupt:
        print("\n诊断结束。")

if __name__ == "__main__":
    main()