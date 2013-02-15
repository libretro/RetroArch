function win = kaiser_window(N, beta)
% Create an N-point kaiser window with given beta.

indices = 2 * (0 : N - 1) / (N - 1) - 1;
mod = modified_bessel(beta);
win = modified_bessel(beta * sqrt(1 - indices.^2)) / mod;