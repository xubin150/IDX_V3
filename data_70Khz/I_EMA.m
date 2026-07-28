%% 1. 一阶移位低通滤波器 (EMA LPF) 频响分析
clear; clc; close all;

Fs = 100; % 采样率 (Hz)
f = linspace(0, Fs/2, 1000); % 分析频率范围：从 0 到 奈奎斯特频率

figure('Name', '一阶 EMA 低通滤波器频响', 'Color', 'w', 'Position', [100, 100, 800, 600]);

% 设定要对比的不同移位位数 (Shift Bits)
shift_bits_list = [1, 2, 3, 4];
colors = lines(length(shift_bits_list));

for i = 1:length(shift_bits_list)
    k = shift_bits_list(i);
    alpha = 1 / (2^k);
    
    % 传递函数系数
    b = alpha;
    a = [1, -(1 - alpha)];
    
    % 计算频率响应
    [h, ~] = freqz(b, a, f, Fs);
    
    % 绘制幅频特性 (增益 dB)
    subplot(2, 1, 1);
    hold on; grid on;
    plot(f, 20*log10(abs(h)), 'Color', colors(i,:), 'LineWidth', 1.5, ...
        'DisplayName', sprintf('Shift = %d (\\alpha = 1/%d)', k, 2^k));
    
    % 绘制相频特性 (相位延迟)
    subplot(2, 1, 2);
    hold on; grid on;
    plot(f, angle(h)*180/pi, 'Color', colors(i,:), 'LineWidth', 1.5, ...
        'DisplayName', sprintf('Shift = %d', k));
end

% 图表格式化
subplot(2, 1, 1);
title('幅频特性 (Gain)');
xlabel('频率 (Hz)'); ylabel('幅值 (dB)');
legend('Location', 'southwest');
ylim([-40 5]);

subplot(2, 1, 2);
title('相频特性 (Phase Delay)');
xlabel('频率 (Hz)'); ylabel('相位角 (度)');
legend('Location', 'southwest');