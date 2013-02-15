function val = modified_bessel(x)

% Mirrors operation as done in RetroArch. Verify accuracy against Matlab's
% implementation.

sum = zeros(size(x));
factorial = ones(size(x));
factorial_mult = zeros(size(x));
x_pow = ones(size(x));
two_div_pow = ones(size(x));
x_sqr = x .* x;

for i = 0 : 17
   sum = sum + x_pow .* two_div_pow ./ (factorial .* factorial);
   factorial_mult = factorial_mult + 1.0;
   x_pow = x_pow .* x_sqr;
   two_div_pow = two_div_pow * 0.25;
   factorial = factorial .* factorial_mult;
end

val = sum;