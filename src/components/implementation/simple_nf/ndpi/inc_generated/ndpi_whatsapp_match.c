/*
 *
 * This file is generated automatically and part of nDPI
 *
 * nDPI is free software: you can redistribute it and/or modify
 * it under the terms of the GNU Lesser General Public License as published by
 * the Free Software Foundation, either version 3 of the License, or
 * (at your option) any later version.
 *
 * nDPI is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU Lesser General Public License for more details.
 *
 * You should have received a copy of the GNU Lesser General Public License
 * along with nDPI.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

/* ****************************************************** */
#include "../ndpi_typedefs.h"
#include "../ndpi_protocol_ids.h"

static ndpi_network ndpi_protocol_whatsapp_protocol_list[] = {
 { 0x0321DD30 /* 3.33.221.48/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x0321FC3D /* 3.33.252.61/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x0FC5CED9 /* 15.197.206.217/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x0FC5D2D0 /* 15.197.210.208/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D403C /* 31.13.64.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4131 /* 31.13.65.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4132 /* 31.13.65.50/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4233 /* 31.13.66.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4238 /* 31.13.66.56/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4334 /* 31.13.67.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D443C /* 31.13.68.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D453C /* 31.13.69.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4631 /* 31.13.70.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4632 /* 31.13.70.50/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4731 /* 31.13.71.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4732 /* 31.13.71.50/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4830 /* 31.13.72.48/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4834 /* 31.13.72.52/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4934 /* 31.13.73.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4A34 /* 31.13.74.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4B3C /* 31.13.75.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4C3C /* 31.13.76.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4D3C /* 31.13.77.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4E3C /* 31.13.78.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4F35 /* 31.13.79.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D4F36 /* 31.13.79.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5030 /* 31.13.80.48/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5035 /* 31.13.80.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5130 /* 31.13.81.48/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5135 /* 31.13.81.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5233 /* 31.13.82.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5237 /* 31.13.82.55/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5331 /* 31.13.83.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5333 /* 31.13.83.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5431 /* 31.13.84.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5433 /* 31.13.84.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5531 /* 31.13.85.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5533 /* 31.13.85.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5631 /* 31.13.86.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5633 /* 31.13.86.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5730 /* 31.13.87.48/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5733 /* 31.13.87.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D583C /* 31.13.88.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5935 /* 31.13.89.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5936 /* 31.13.89.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5A3C /* 31.13.90.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5B3C /* 31.13.91.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5C30 /* 31.13.92.48/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5C34 /* 31.13.92.52/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5D35 /* 31.13.93.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5D36 /* 31.13.93.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5E34 /* 31.13.94.52/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5E36 /* 31.13.94.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x1F0D5F3C /* 31.13.95.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x22C0B50C /* 34.192.181.12/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x22C12670 /* 34.193.38.112/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x22C247D9 /* 34.194.71.217/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x22C2FFE6 /* 34.194.255.230/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x45ABFA3C /* 69.171.250.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846036 /* 102.132.96.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846136 /* 102.132.97.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684623C /* 102.132.98.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684633C /* 102.132.99.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684643C /* 102.132.100.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684653C /* 102.132.101.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684663C /* 102.132.102.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684673C /* 102.132.103.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684683C /* 102.132.104.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x6684693C /* 102.132.105.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846A3C /* 102.132.106.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846B3C /* 102.132.107.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846C3C /* 102.132.108.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846D3C /* 102.132.109.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846E3C /* 102.132.110.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x66846F3C /* 102.132.111.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0003C /* 157.240.0.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0013C /* 157.240.1.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00235 /* 157.240.2.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00236 /* 157.240.2.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00336 /* 157.240.3.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0043C /* 157.240.4.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0053C /* 157.240.5.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00635 /* 157.240.6.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00636 /* 157.240.6.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00735 /* 157.240.7.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00736 /* 157.240.7.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00835 /* 157.240.8.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00836 /* 157.240.8.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00935 /* 157.240.9.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00936 /* 157.240.9.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00A35 /* 157.240.10.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00A36 /* 157.240.10.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00B35 /* 157.240.11.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00B36 /* 157.240.11.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00C35 /* 157.240.12.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00C36 /* 157.240.12.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00D36 /* 157.240.13.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00E34 /* 157.240.14.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF00F3C /* 157.240.15.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01034 /* 157.240.16.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0113C /* 157.240.17.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01234 /* 157.240.18.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01335 /* 157.240.19.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01336 /* 157.240.19.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01434 /* 157.240.20.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01534 /* 157.240.21.52/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01635 /* 157.240.22.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01636 /* 157.240.22.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01735 /* 157.240.23.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01736 /* 157.240.23.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0183C /* 157.240.24.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0193C /* 157.240.25.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01A36 /* 157.240.26.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01B36 /* 157.240.27.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01C33 /* 157.240.28.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01C37 /* 157.240.28.55/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01D3C /* 157.240.29.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01E36 /* 157.240.30.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF01F3C /* 157.240.31.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C034 /* 157.240.192.52/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C037 /* 157.240.192.55/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C13C /* 157.240.193.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C236 /* 157.240.194.54/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C336 /* 157.240.195.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C338 /* 157.240.195.56/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C43C /* 157.240.196.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C53C /* 157.240.197.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C63C /* 157.240.198.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C73C /* 157.240.199.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C83C /* 157.240.200.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0C93C /* 157.240.201.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0CA3C /* 157.240.202.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0CB3C /* 157.240.203.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0CC3C /* 157.240.204.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0CD3C /* 157.240.205.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0CE3C /* 157.240.206.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0CF3C /* 157.240.207.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D03C /* 157.240.208.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D13C /* 157.240.209.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D23C /* 157.240.210.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D33C /* 157.240.211.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D43C /* 157.240.212.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D53C /* 157.240.213.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D63C /* 157.240.214.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D73C /* 157.240.215.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D83C /* 157.240.216.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0D93C /* 157.240.217.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0DA3C /* 157.240.218.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0DB3C /* 157.240.219.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0DC3C /* 157.240.220.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0DD3C /* 157.240.221.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0DE3C /* 157.240.222.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0DF3C /* 157.240.223.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E03C /* 157.240.224.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E13C /* 157.240.225.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E23C /* 157.240.226.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E33C /* 157.240.227.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E43C /* 157.240.228.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E53C /* 157.240.229.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E73C /* 157.240.231.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E83C /* 157.240.232.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0E93C /* 157.240.233.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0EA3C /* 157.240.234.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0EB3C /* 157.240.235.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0EC3C /* 157.240.236.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0ED3C /* 157.240.237.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0EE3C /* 157.240.238.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0EF3C /* 157.240.239.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F03C /* 157.240.240.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F13C /* 157.240.241.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F23C /* 157.240.242.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F33C /* 157.240.243.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F43C /* 157.240.244.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F53C /* 157.240.245.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F63C /* 157.240.246.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F73C /* 157.240.247.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F83C /* 157.240.248.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0F93C /* 157.240.249.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0FA3C /* 157.240.250.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0FB3C /* 157.240.251.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0FC3C /* 157.240.252.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0FD3C /* 157.240.253.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0x9DF0FE3C /* 157.240.254.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346803C /* 163.70.128.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346813C /* 163.70.129.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346823C /* 163.70.130.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346833C /* 163.70.131.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346843C /* 163.70.132.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346853C /* 163.70.133.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346863C /* 163.70.134.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346873C /* 163.70.135.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346883C /* 163.70.136.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346893C /* 163.70.137.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3468A3C /* 163.70.138.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3468B3C /* 163.70.139.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3468C3C /* 163.70.140.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3468D3C /* 163.70.141.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3468E3C /* 163.70.142.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3468F3C /* 163.70.143.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346903C /* 163.70.144.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346913C /* 163.70.145.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346923C /* 163.70.146.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346933C /* 163.70.147.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346943C /* 163.70.148.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346953C /* 163.70.149.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346963C /* 163.70.150.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346973C /* 163.70.151.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346983C /* 163.70.152.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA346993C /* 163.70.153.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3469A3C /* 163.70.154.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3469B3C /* 163.70.155.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3469C3C /* 163.70.156.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3469D3C /* 163.70.157.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3469E3C /* 163.70.158.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xA3469F3C /* 163.70.159.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC031 /* 179.60.192.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC033 /* 179.60.192.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC13C /* 179.60.193.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC235 /* 179.60.194.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC236 /* 179.60.194.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC331 /* 179.60.195.49/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB33CC333 /* 179.60.195.51/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CD835 /* 185.60.216.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CD836 /* 185.60.216.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CD935 /* 185.60.217.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CD936 /* 185.60.217.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CDA35 /* 185.60.218.53/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CDA36 /* 185.60.218.54/32 */, 32, NDPI_PROTOCOL_WHATSAPP },
 { 0xB93CDB3C /* 185.60.219.60/31 */, 31, NDPI_PROTOCOL_WHATSAPP },
 /* End */
 { 0x0, 0, 0 }
};
