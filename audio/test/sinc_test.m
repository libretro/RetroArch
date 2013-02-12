% MATLAB test case for RetroArch SINC upsampler.
close all;

% 4-tap and 8-tap are Lanczos windowed, but include here for completeness.
sidelobes = [2 4 8 32 128];
taps = sidelobes * 2;

phases = 256;
cutoffs = [0.65 0.75 0.825 0.90 0.95];
betas = [2.0 3.0 5.5 10.5 14.5];

freqs = 0.05 : 0.05 : 0.99;

filters = length(taps);
for i = 1 : filters
    filter_length = taps(i) * phases;
    
    % Generate SINC.
    sinc_indices = 2 * ((0 : (filter_length - 1)) / (filter_length - 1)) - 1;
    s = cutoffs(i) * sinc(cutoffs(i) * sinc_indices * sidelobes(i));
    win = kaiser(filter_length, betas(i))';
    filter = s .* win;

    impulse_response_half = 0.5 * upfirdn(1, filter, phases, phases / 2);
    figure('name', sprintf('Response SINC: %d taps', taps(i)));
    freqz(impulse_response_half);
    ylim([-200 0]);

    signal = zeros(1, 4001);
    for freq = freqs
        signal = signal + sin(pi * freq * (0 : 4000));
    end
    
    resampled = upfirdn(signal, filter, phases, round(phases * 44100 / 48000));
    figure('name', sprintf('Kaiser SINC: %d taps, w = %.f', taps(i), freq));
    freqz(resampled .* kaiser(length(resampled), 40.0)');
    ylim([-80 70]);
end