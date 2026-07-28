%% 2. 二阶 IIR 带通滤波器 (Biquad) 定点化分析
clear; clc;

Fs = 100; % 采样率 (Hz)
f = linspace(0, Fs/2, 2000);

% --- 参数设置区 ---
F_center = 5; % 中心频率 5Hz
F_bw = 4;      % 带宽 4Hz (即通带大约为 3Hz ~ 7Hz)

% 使用 butter 函数设计 1阶带通 (产生 2阶 Biquad 滤波器)
% 归一化频率 = 实际频率 / (Fs/2)
Wn = [(F_center - F_bw/2), (F_center + F_bw/2)] / (Fs/2);
[b_float, a_float] = butter(1, Wn, 'bandpass'); 

% --- Q15 定点化处理 ---
Q_FORMAT = 15;
scale = 2^Q_FORMAT;

% 对系数乘以 2^15 并四舍五入取整
b_q15 = round(b_float * scale);
a_q15 = round(a_float * scale);

% 为了在 MATLAB 里验证定点化后的频响，再将整数除以 2^15 还原成浮点
b_quantized = b_q15 / scale;
a_quantized = a_q15 / scale;

% --- 绘制对比图 ---
% 计算频率响应
[h_flt, ~] = freqz(b_float, a_float, f, Fs);
[h_qnt, ~] = freqz(b_quantized, a_quantized, f, Fs);

figure('Name', '二阶带通滤波器 Bode 图', 'Color', 'w', 'Position', [150, 150, 800, 600]);

% 1. 绘制幅频特性
subplot(2, 1, 1);
plot(f, 20*log10(abs(h_flt)), 'b', 'LineWidth', 2, 'DisplayName', '理想浮点设计');
hold on; grid on;
plot(f, 20*log10(abs(h_qnt)), 'r--', 'LineWidth', 1.5, 'DisplayName', 'Q15 定点化实际效果');
title(sprintf('幅频特性 (Gain) - 中心频率 %.1fHz, 带宽 %.1fHz', F_center, F_bw));
ylabel('幅值 (dB)');
legend('Location', 'southwest');
xlim([0 40]);
ylim([-50 5]);

% 2. 绘制相频特性
subplot(2, 1, 2);
plot(f, angle(h_flt)*180/pi, 'b', 'LineWidth', 2);
hold on; grid on;
plot(f, angle(h_qnt)*180/pi, 'r--', 'LineWidth', 1.5);
% 标记 0 度线
yline(0, 'k-', 'LineWidth', 1);
title('相频特性 (Phase Delay)');
xlabel('频率 (Hz)'); ylabel('相位角 (度)');
xlim([0 40]);
ylim([-100 100]);
% --- 自动生成 C 语言宏定义 ---
fprintf('=========== STM32 C语言系数宏定义 ===========\n');
fprintf('// 采样率: %.1f Hz | 目标中心频率: %.1f Hz\n', Fs, F_center);
fprintf('#define SHIFT_BITS %d\n', Q_FORMAT);
fprintf('#define B0  %d\n', b_q15(1));
fprintf('#define B1  %d\n', b_q15(2));
fprintf('#define B2  %d\n', b_q15(3));
% 注意：MATLAB 的 a_float(1) 始终是 1，C代码中我们只用 a1 和 a2，并且符号取反或一致取决于你的C公式
fprintf('#define A1  %d\n', a_q15(2)); 
fprintf('#define A2  %d\n', a_q15(3));
fprintf('=============================================\n');