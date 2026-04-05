import serial
import pygame
from pygame.locals import *
from OpenGL.GL import *
from OpenGL.GLU import *
import math
import time

# --- 配置 ---
PORT = 'COM12' 
BAUD = 115200
ALPHA = 0.98   
MAG_DECLINATION = -2.6  # 磁偏角

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
except:
    print(f"无法打开串口 {PORT}"); exit()

# --- 全局变量 ---
mag_offsets = [0, 0, 0]  
mag_min = [9999, 9999, 9999]
mag_max = [-9999, -9999, -9999]
is_calibrating = False

# --- 2D 文字显示辅助 ---
def draw_text(position, text):
    font = pygame.font.SysFont('Consolas', 22)
    text_surface = font.render(text, True, (255, 255, 0), (0, 0, 0, 150))
    text_data = pygame.image.tostring(text_surface, "RGBA", True)
    glWindowPos2d(position[0], position[1])
    glDrawPixels(text_surface.get_width(), text_surface.get_height(), GL_RGBA, GL_UNSIGNED_BYTE, text_data)

def draw_axes():
    glLineWidth(3)
    glBegin(GL_LINES)
    glColor3f(1.0, 0.0, 0.0); glVertex3f(0, 0, 0); glVertex3f(3, 0, 0) # X-红
    glColor3f(0.0, 1.0, 0.0); glVertex3f(0, 0, 0); glVertex3f(0, 3, 0) # Y-绿
    glColor3f(0.0, 0.0, 1.0); glVertex3f(0, 0, 0); glVertex3f(0, 0, 3) # Z-蓝
    glEnd()

def draw_boat():
    glBegin(GL_QUADS)
    glColor3f(0.2, 0.6, 1.0) # 船身顶
    glVertex3f(2, 0.3, -1); glVertex3f(-2, 0.3, -1); glVertex3f(-2, 0.3, 1); glVertex3f(2, 0.3, 1)
    glColor3f(0.3, 0.3, 0.3) # 船底
    glVertex3f(1.5, -0.6, 0.8); glVertex3f(-1.5, -0.6, 0.8); glVertex3f(-1.5, -0.6, -0.8); glVertex3f(1.5, -0.6, -0.8)
    glColor3f(1.0, 0.4, 0.0) # 船侧面
    glVertex3f(2, 0.3, 1); glVertex3f(2, 0.3, -1); glVertex3f(1.5, -0.6, -0.8); glVertex3f(1.5, -0.6, 0.8)
    glEnd()

def calculate_tilt_compensated_yaw(roll, pitch, mx, my, mz):
    phi = math.radians(roll)
    theta = math.radians(pitch)

    # 倾斜补偿投影公式
    Xh = mx * math.cos(theta) + my * math.sin(phi) * math.sin(theta) + mz * math.cos(phi) * math.sin(theta)
    Yh = my * math.cos(phi) - mz * math.sin(phi)

    yaw = math.degrees(math.atan2(-Yh, Xh))
    return (yaw + MAG_DECLINATION + 360) % 360

def main():
    global is_calibrating, mag_min, mag_max, mag_offsets
    pygame.init()
    display = (1200, 800)
    pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    pygame.display.set_caption("H743 + BMI088 + BMM150 稳定版")
    
    glEnable(GL_DEPTH_TEST)
    glMatrixMode(GL_PROJECTION)
    gluPerspective(45, (display[0] / display[1]), 0.1, 100.0)
    glMatrixMode(GL_MODELVIEW)
    
    comp_roll, comp_pitch, comp_yaw = 0.0, 0.0, 0.0
    last_time = time.time()
    clock = pygame.time.Clock()

    # 1. 陀螺仪零偏校准
    print("请保持传感器静止，正在校准陀螺仪...")
    gx_bias, gy_bias, gz_bias = 0, 0, 0
    samples = 100
    count = 0
    while count < samples:
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line.startswith("DATA:"):
            parts = line.replace("DATA:", "").split(',')
            if len(parts) >= 6:
                gx_bias += int(parts[3]) / 1000.0
                gy_bias += int(parts[4]) / 1000.0
                gz_bias += int(parts[5]) / 1000.0
                count += 1
    gx_bias /= samples; gy_bias /= samples; gz_bias /= samples
    print(f"校准完成！GZ 零偏: {gz_bias:.4f}")

    while True:
        if ser.in_waiting > 200: ser.reset_input_buffer()
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        
        current_time = time.time()
        dt = current_time - last_time
        last_time = current_time

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                ser.close(); pygame.quit(); quit()
            if event.type == pygame.KEYDOWN:
                if event.key == pygame.K_c: # 按C键开始/停止磁力计校准
                    is_calibrating = not is_calibrating
                    if not is_calibrating:
                        mag_offsets = [(mag_max[i] + mag_min[i]) / 2 for i in range(3)]
                        print(f"校准结果: {mag_offsets}")

        if line.startswith("DATA:"):
            try:
                parts = line.replace("DATA:", "").split(',')
                if len(parts) >= 9:
                    ax, ay, az = [int(x)/1000.0 for x in parts[:3]]
                    gx, gy, gz = [int(x)/1000.0 for x in parts[3:6]]
                    mx_raw, my_raw, mz_raw = [int(x) for x in parts[6:9]]

                    if is_calibrating:
                        mag_min = [min(mag_min[i], [mx_raw, my_raw, mz_raw][i]) for i in range(3)]
                        mag_max = [max(mag_max[i], [mx_raw, my_raw, mz_raw][i]) for i in range(3)]
                    
                    # --- 物理对齐映射 ---
                    mx_o = mx_raw - mag_offsets[0]
                    my_o = my_raw - mag_offsets[1]
                    mz_o = mz_raw - mag_offsets[2]

                    # 对齐逻辑: IMU_X=BMM_Y, IMU_Y=BMM_X, IMU_Z=-BMM_Z
                    # 修正右手系：my 需取反
                    mx =  my_o
                    my =  mx_o 
                    mz = -mz_o
                    
                    # 基础姿态计算
                    acc_roll = math.degrees(math.atan2(ay, az))
                    acc_pitch = -math.degrees(math.atan2(-ax, math.sqrt(ay*ay + az*az)))
                    
                    comp_roll = ALPHA * (comp_roll + (gx - gx_bias) * dt) + (1 - ALPHA) * acc_roll
                    comp_pitch = ALPHA * (comp_pitch - (gy - gy_bias) * dt) + (1 - ALPHA) * acc_pitch

                    # 磁力计计算与融合
                    mag_yaw = calculate_tilt_compensated_yaw(comp_roll, -comp_pitch, mx, my, mz)
                    
                    yaw_error = mag_yaw - comp_yaw
                    if yaw_error > 180: yaw_error -= 360
                    if yaw_error < -180: yaw_error += 360
                    
                    YAW_ALPHA = 0.99
                    gz_fixed = gz - gz_bias
                    # 减号：处理顺时针 gz 为负的极性
                    comp_yaw = (comp_yaw + gz_fixed * dt) + (1 - YAW_ALPHA) * yaw_error 
                    comp_yaw %= 360

            except Exception as e:
                pass

        # --- 渲染 ---
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        
        # 绘制背景 UI 文字
        draw_text((20, 760), f"ROLL:  {comp_roll:>6.2f}")
        draw_text((20, 730), f"PITCH: {comp_pitch:>6.2f}")
        draw_text((20, 700), f"YAW:   {comp_yaw:>6.2f}")
        status_color = "CALIBRATING..." if is_calibrating else "READY (Press 'C' to Calibrate)"
        draw_text((20, 20), f"STATUS: {status_color}")

        glTranslatef(0.0, 0.0, -8)
        glPushMatrix()
        glRotatef(comp_yaw, 0, 1, 0)    
        glRotatef(comp_pitch, 0, 0, 1)  
        glRotatef(comp_roll, 1, 0, 0)   
        draw_boat(); draw_axes()
        glPopMatrix()
        
        pygame.display.flip()
        clock.tick(60)

if __name__ == "__main__":
    main()