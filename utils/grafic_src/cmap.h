/*
 This is a simple 256-color table with 0=black and 255=white, and a
 spectrum blue-green-yellow-orange-red-pink in between, corresponding
 to lpc.hdf.
*/

int ic;
unsigned char cmap[768];
int icmap[768] = {
  0,   0,   0,
  0,   0,   0,
  0,   0,   6,
  0,   0,  12,
  0,   0,  18,
  0,   0,  24,
  0,   0,  30,
  0,   0,  36,
  0,   0,  42,
  0,   0,  48,
  0,   0,  54,
  0,   0,  60,
  0,   0,  66,
  0,   0,  72,
  0,   0,  78,
  0,   0,  85,
  0,   0,  91,
  0,   0,  97,
  0,   0, 103,
  0,   0, 109,
  0,   0, 115,
  0,   0, 121,
  0,   0, 127,
  0,   0, 133,
  0,   0, 139,
  0,   0, 145,
  0,   0, 151,
  0,   0, 157,
  0,   0, 163,
  0,   0, 170,
  0,   0, 176,
  0,   0, 182,
  0,   0, 188,
  0,   0, 194,
  0,   0, 200,
  0,   0, 206,
  0,   0, 212,
  0,   0, 218,
  0,   0, 224,
  0,   0, 230,
  0,   0, 236,
  0,   0, 242,
  0,   0, 248,
  0,   0, 255,
  0,   6, 255,
  0,  12, 255,
  0,  18, 255,
  0,  24, 255,
  0,  30, 255,
  0,  36, 255,
  0,  42, 255,
  0,  48, 255,
  0,  54, 255,
  0,  60, 255,
  0,  66, 255,
  0,  72, 255,
  0,  78, 255,
  0,  85, 255,
  0,  91, 255,
  0,  97, 255,
  0, 103, 255,
  0, 109, 255,
  0, 115, 255,
  0, 121, 255,
  0, 127, 255,
  0, 133, 255,
  0, 139, 255,
  0, 145, 255,
  0, 151, 255,
  0, 157, 255,
  0, 163, 255,
  0, 170, 255,
  0, 176, 255,
  0, 182, 255,
  0, 188, 255,
  0, 194, 255,
  0, 200, 255,
  0, 206, 255,
  0, 212, 255,
  0, 218, 255,
  0, 224, 255,
  0, 230, 255,
  0, 236, 255,
  0, 242, 255,
  0, 248, 255,
  0, 255, 255,
  0, 255, 248,
  0, 255, 242,
  0, 255, 236,
  0, 255, 230,
  0, 255, 224,
  0, 255, 218,
  0, 255, 212,
  0, 255, 206,
  0, 255, 200,
  0, 255, 194,
  0, 255, 188,
  0, 255, 182,
  0, 255, 176,
  0, 255, 170,
  0, 255, 163,
  0, 255, 157,
  0, 255, 151,
  0, 255, 145,
  0, 255, 139,
  0, 255, 133,
  0, 255, 127,
  0, 255, 121,
  0, 255, 115,
  0, 255, 109,
  0, 255, 103,
  0, 255,  97,
  0, 255,  91,
  0, 255,  85,
  0, 255,  78,
  0, 255,  72,
  0, 255,  66,
  0, 255,  60,
  0, 255,  54,
  0, 255,  48,
  0, 255,  42,
  0, 255,  36,
  0, 255,  30,
  0, 255,  24,
  0, 255,  18,
  0, 255,  12,
  0, 255,   6,
  0, 255,   0,
  6, 255,   0,
 12, 255,   0,
 18, 255,   0,
 24, 255,   0,
 30, 255,   0,
 36, 255,   0,
 42, 255,   0,
 48, 255,   0,
 54, 255,   0,
 60, 255,   0,
 66, 255,   0,
 72, 255,   0,
 78, 255,   0,
 85, 255,   0,
 91, 255,   0,
 97, 255,   0,
103, 255,   0,
109, 255,   0,
115, 255,   0,
121, 255,   0,
127, 255,   0,
133, 255,   0,
139, 255,   0,
145, 255,   0,
151, 255,   0,
157, 255,   0,
163, 255,   0,
170, 255,   0,
176, 255,   0,
182, 255,   0,
188, 255,   0,
194, 255,   0,
200, 255,   0,
206, 255,   0,
212, 255,   0,
218, 255,   0,
224, 255,   0,
230, 255,   0,
236, 255,   0,
242, 255,   0,
248, 255,   0,
255, 255,   0,
255, 248,   0,
255, 242,   0,
255, 236,   0,
255, 230,   0,
255, 224,   0,
255, 218,   0,
255, 212,   0,
255, 206,   0,
255, 200,   0,
255, 194,   0,
255, 188,   0,
255, 182,   0,
255, 176,   0,
255, 170,   0,
255, 163,   0,
255, 157,   0,
255, 151,   0,
255, 145,   0,
255, 139,   0,
255, 133,   0,
255, 127,   0,
255, 121,   0,
255, 115,   0,
255, 109,   0,
255, 103,   0,
255,  97,   0,
255,  91,   0,
255,  85,   0,
255,  78,   0,
255,  72,   0,
255,  66,   0,
255,  60,   0,
255,  54,   0,
255,  48,   0,
255,  42,   0,
255,  36,   0,
255,  30,   0,
255,  24,   0,
255,  18,   0,
255,  12,   0,
255,   6,   0,
255,   0,   0,
255,   6,   6,
255,  12,  12,
255,  18,  18,
255,  24,  24,
255,  30,  30,
255,  36,  36,
255,  42,  42,
255,  48,  48,
255,  54,  54,
255,  60,  60,
255,  66,  66,
255,  72,  72,
255,  78,  78,
255,  85,  85,
255,  91,  91,
255,  97,  97,
255, 103, 103,
255, 109, 109,
255, 115, 115,
255, 121, 121,
255, 127, 127,
255, 133, 133,
255, 139, 139,
255, 145, 145,
255, 151, 151,
255, 157, 157,
255, 163, 163,
255, 170, 170,
255, 176, 176,
255, 182, 182,
255, 188, 188,
255, 194, 194,
255, 200, 200,
255, 206, 206,
255, 212, 212,
255, 218, 218,
255, 224, 224,
255, 230, 230,
255, 236, 236,
255, 242, 242,
255, 248, 248,
255, 255, 255,
255, 255, 255,
255, 255, 255
};
for (ic=0; ic<768; ic++) cmap[ic] = icmap[ic];
