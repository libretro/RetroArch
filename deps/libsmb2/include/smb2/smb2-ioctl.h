/*
 * Copyright 2025 Ronnie Sahlberg <ronniesahberg@gmail.com>
 *
 * Permission is hereby granted, free of charge, to any person
 * obtaining a copy of this software and associated documentation
 * files (the “Software”), to deal in the Software without
 * restriction, including without limitation the rights to use,
 * copy, modify, merge, publish, distribute, sublicense, and/or
 * sell copies of the Software, and to permit persons to whom
 * the Software is furnished to do so, subject to the following
 * conditions:
 *
 * The above copyright notice and this permission notice shall
 * be included in all copies or substantial portions of the
 * Software.
 *
 * THE SOFTWARE IS PROVIDED “AS IS”, WITHOUT WARRANTY OF ANY
 * KIND, EXPRESS OR IMPLIED, INCLUDING BUT NOT LIMITED TO THE
 * WARRANTIES OF MERCHANTABILITY, FITNESS FOR A PARTICULAR
 * PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE AUTHORS
 * OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR
 * OTHER LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR
 * OTHERWISE, ARISING FROM, OUT OF OR IN CONNECTION WITH THE
 * SOFTWARE OR THE USE OR OTHER DEALINGS IN THE SOFTWARE.
 */
/*
 * Or optionally just relicense this file to whatever license you want.
 */
#ifndef _SMB2_IOCTL_H_
#define _SMB2_IOCTL_H_

#define FSCTL_CREATE_OR_GET_OBJECT_ID              0x000900C0
#define FSCTL_DELETE_OBJECT_ID                     0x000900A0
#define FSCTL_DELETE_REPARSE_POINT                 0x000900AC
#define FSCTL_DUPLICATE_EXTENTS_TO_FILE            0x00098344
#define FSCTL_DUPLICATE_EXTENTS_TO_FILE_EX         0x000983E8 
#define FSCTL_FILESYSTEM_GET_STATISTICS            0x00090060
#define FSCTL_FILE_LEVEL_TRIM                      0x00098208
#define FSCTL_FIND_FILES_BY_SID                    0x0009008F
#define FSCTL_GET_COMPRESSION                      0x0009003C
#define FSCTL_GET_INTEGRITY_INFORMATION            0x0009027C
#define FSCTL_GET_NTFS_VOLUME_DATA                 0x00090064
#define FSCTL_GET_REFS_VOLUME_DATA                 0x000902D8
#define FSCTL_GET_OBJECT_ID                        0x0009009C
#define FSCTL_GET_REPARSE_POINT                    0x000900A8
#define FSCTL_GET_RETRIEVAL_POINTER_COUNT          0x0009042B
#define FSCTL_GET_RETRIEVAL_POINTERS               0x00090073
#define FSCTL_GET_RETRIEVAL_POINTERS_AND_REFCOUNT  0x000903D3
#define FSCTL_IS_PATHNAME_VALID                    0x0009002C
#define FSCTL_LMR_SET_LINK_TRACKING_INFORMATION    0x001400EC
#define FSCTL_MARK_HANDLE                          0x000900FC
#define FSCTL_OFFLOAD_READ                         0x00094264
#define FSCTL_OFFLOAD_WRITE                        0x00098268
#define FSCTL_PIPE_PEEK                            0x0011400C
#define FSCTL_PIPE_TRANSCEIVE                      0x0011C017
#define FSCTL_PIPE_WAIT                            0x00110018
#define FSCTL_QUERY_ALLOCATED_RANGES               0x000940CF
#define FSCTL_QUERY_FAT_BPB                        0x00090058
#define FSCTL_QUERY_FILE_REGIONS                   0x00090284
#define FSCTL_QUERY_ON_DISK_VOLUME_INFO            0x0009013C
#define FSCTL_QUERY_SPARING_INFO                   0x00090138
#define FSCTL_READ_FILE_USN_DATA                   0x000900EB
#define FSCTL_RECALL_FILE                          0x00090117
#define FSCTL_REFS_STREAM_SNAPSHOT_MANAGEMENT      0x00090440
#define FSCTL_SET_COMPRESSION                      0x0009C040
#define FSCTL_SET_DEFECT_MANAGEMENT                0x00098134
#define FSCTL_SET_ENCRYPTION                       0x000900D7
#define FSCTL_SET_INTEGRITY_INFORMATION            0x0009C280
#define FSCTL_SET_INTEGRITY_INFORMATION_EX         0x00090380
#define FSCTL_SET_OBJECT_ID                        0x00090098
#define FSCTL_SET_OBJECT_ID_EXTENDED               0x000900BC
#define FSCTL_SET_REPARSE_POINT                    0x000900A4
#define FSCTL_SET_SPARSE                           0x000900C4
#define FSCTL_SET_ZERO_DATA                        0x000980C8
#define FSCTL_SET_ZERO_ON_DEALLOCATION             0x00090194
#define FSCTL_SIS_COPYFILE                         0x00090100
#define FSCTL_WRITE_USN_CLOSE_RECORD               0x000900EF

#define FSCTL_SRV_ENUMERATE_SNAPSHOTS              0x00144064
#define FSCTL_GET_SHADOW_COPY_DATA                 0x00144064


#endif /* _SMB2_IOCTL_H_ */
