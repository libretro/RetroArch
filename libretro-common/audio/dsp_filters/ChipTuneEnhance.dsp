filters = 4
filter0 = eq
filter1 = reverb
filter2 = iir
filter3 = panning

eq_frequencies = "32 64 125 250 500 1000 2000 4000 8000 16000 20000"
eq_gains = "6 9 12 7 6 5 7 9 11 6 0"

# Reverb - slight reverb
 reverb_drytime = 0.5
 reverb_wettime = 0.15
 reverb_damping = 0.8
 reverb_roomwidth = 0.25
 reverb_roomsize = 0.25

# IIR - filters out some harsh sounds on the upper end
iir_type = RIAA_CD

# Panning - cut the volume a bit
panning_left_mix = "0.75 0.0"
panning_right_mix = "0.0 0.75"
