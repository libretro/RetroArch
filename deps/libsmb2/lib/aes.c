
/*
   Copyright (C) 2025 by Ronnie Sahlberg <ronniesahlberg@gmail.com>

   This program is free software; you can redistribute it and/or modify
   it under the terms of the GNU Lesser General Public License as published by
   the Free Software Foundation; either version 2.1 of the License, or
   (at your option) any later version.

   This program is distributed in the hope that it will be useful,
   but WITHOUT ANY WARRANTY; without even the implied warranty of
   MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
   GNU Lesser General Public License for more details.

   You should have received a copy of the GNU Lesser General Public License
   along with this program; if not, see <http://www.gnu.org/licenses/>.
*/

#include "aes.h"

#ifdef __APPLE__
#include "aes_apple.h"
#else
#include "aes_reference.h"
#endif

void AES128_ECB_encrypt(uint8_t* input, const uint8_t* key, uint8_t *output) {
#ifdef __APPLE__
AES128_ECB_encrypt_apple(input, key, output);
#else
AES128_ECB_encrypt_reference(input, key, output);
#endif
}
