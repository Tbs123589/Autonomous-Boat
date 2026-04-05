import serial
import pygame
from pygame.locals import *
from OpenGL.GL import *
from OpenGL.GLU import *
import math
import time

# --- 配置 ---
PORT = 'COM12'  # 改为你的 COM 口
BAUD = 115200
ALPHA = 0.96   # 互补滤波系数

try:
    ser = serial.Serial(PORT, BAUD, timeout=0.1)
except:
    print(f"无法打开串口 {PORT}")
    exit()

def draw_axes():
    """绘制 XYZ 轴正方向：红=X, 绿=Y, 蓝=Z"""
    glLineWidth(3)
    glBegin(GL_LINES)
    # X 轴 - 红色 (指向船头)
    glColor3f(1.0, 0.0, 0.0)
    glVertex3f(0, 0, 0); glVertex3f(3, 0, 0)
    # Y 轴 - 绿色 (侧翻)
    glColor3f(0.0, 1.0, 0.0)
    glVertex3f(0, 0, 0); glVertex3f(0, 3, 0)
    # Z 轴 - 蓝色 (垂直)
    glColor3f(0.0, 0.0, 1.0)
    glVertex3f(0, 0, 0); glVertex3f(0, 0, 3)
    glEnd()
    glLineWidth(1)

def draw_boat():
    """绘制彩色船体"""
    glBegin(GL_QUADS)
    # 船身 - 亮蓝色
    glColor3f(0.2, 0.6, 1.0)
    glVertex3f(1, 0.3, -2); glVertex3f(-1, 0.3, -2); glVertex3f(-1, 0.3, 2); glVertex3f(1, 0.3, 2)
    # 船底 - 灰色
    glColor3f(0.4, 0.4, 0.4)
    glVertex3f(1, -0.4, 2); glVertex3f(-1, -0.4, 2); glVertex3f(-1, -0.4, -2); glVertex3f(1, -0.4, -2)
    # 侧面 - 橙色 (增加辨识度)
    glColor3f(1.0, 0.5, 0.0)
    glVertex3f(1, 0.3, 2); glVertex3f(1, 0.3, -2); glVertex3f(1, -0.4, -2); glVertex3f(1, -0.4, 2)
    glVertex3f(-1, 0.3, 2); glVertex3f(-1, 0.3, -2); glVertex3f(-1, -0.4, -2); glVertex3f(-1, -0.4, 2)
    glEnd()

def draw_text(window, font, x, y, text):
    """在屏幕上绘制 2D 文字"""
    text_surface = font.render(text, True, (255, 255, 255)) # 白色文字
    window.blit(text_surface, (x, y))

def main():
    pygame.init()
    display = (1200, 800)
    # 使用 OPENGL 和 DOUBLEBUF
    window = pygame.display.set_mode(display, DOUBLEBUF | OPENGL)
    pygame.display.set_caption("BMI088 无人船高清姿态演示")
    
    # 2D 文字渲染层
    font = pygame.font.SysFont('arial', 28)
    text_overlay = pygame.Surface(display, pygame.SRCALPHA)

    # OpenGL 初始化
    glClearColor(0.05, 0.1, 0.2, 1.0) # 背景改为深蓝黑色
    glEnable(GL_DEPTH_TEST)
    glMatrixMode(GL_PROJECTION)
    gluPerspective(45, (display[0] / display[1]), 0.1, 100.0)
    glMatrixMode(GL_MODELVIEW)
    
    # 角度变量
    comp_roll, comp_pitch = 0.0, 0.0
    last_time = time.time()
    
    clock = pygame.time.Clock()

    while True:
        dt = time.time() - last_time
        last_time = time.time()

        for event in pygame.event.get():
            if event.type == pygame.QUIT:
                ser.close()
                pygame.quit()
                quit()

        # 串口数据解析
        line = ser.readline().decode('utf-8', errors='ignore').strip()
        if line.startswith("DATA:"):
            try:
                parts = line.replace("DATA:", "").split(',')
                if len(parts) == 6:
                    ax, ay, az = [int(x)/1000.0 for x in parts[:3]]
                    gx, gy, gz = [int(x)/1000.0 for x in parts[3:]]
                    
                    # 加速度计计算静态角度
                    acc_roll = math.atan2(ay, az) * 57.2957
                    acc_pitch = math.atan2(-ax, math.sqrt(ay*ay + az*az)) * 57.2957
                    
                    # 互补滤波 (注意：根据 BMI088 安装方向，gx/gy 可能需要取反)
                    comp_roll = ALPHA * (comp_roll + gx * dt) + (1 - ALPHA) * acc_roll
                    comp_pitch = ALPHA * (comp_pitch + gy * dt) + (1 - ALPHA) * acc_pitch
            except: pass

        # --- 3. 渲染阶段 ---
        glClear(GL_COLOR_BUFFER_BIT | GL_DEPTH_BUFFER_BIT)
        glLoadIdentity()
        glTranslatef(0.0, 0.0, -12) # 相机后移

        # 绘制背景网格
        glColor3f(0.2, 0.3, 0.4)
        glBegin(GL_LINES)
        for i in range(-15, 16):
            glVertex3f(i, -3, -15); glVertex3f(i, -3, 15)
            glVertex3f(-15, -3, i); glVertex3f(15, -3, i)
        glEnd()

        # 旋转船体和坐标轴
        glPushMatrix()
        glRotatef(comp_roll, 0, 0, 1)    # 绕 Z 轴横滚
        glRotatef(comp_pitch, 1, 0, 0)  # 绕 X 轴俯仰
        
        draw_boat()  # 画船
        draw_axes()  # 画 XYZ 轴正方向
        glPopMatrix()

        # --- 4. 2D 文字层处理 ---
        # 必须在 OpenGL 绘制后，切换回 2D 模式或使用 pygame 覆盖
        glFlush()
        # 简单处理：通过 title 显示或覆盖（这里演示最稳妥的 title 方案）
        pygame.display.set_caption(f"BMI088 | Roll: {comp_roll:.1f}° | Pitch: {comp_pitch:.1f}°")

        pygame.display.flip()
        clock.tick(100)

if __name__ == "__main__":
    main()