// MAC.h
// Runs on LM3S8962
// Low-level Ethernet interface

/* This example accompanies the book
   Embedded Systems: Real-Time Operating Systems for the Arm Cortex-M3, Volume 3,  
   ISBN: 978-1466468863, Jonathan Valvano, copyright (c) 2012

   Program 9.1, section 9.3

 Copyright 2012 by Jonathan W. Valvano, valvano@mail.utexas.edu
    You may use, edit, run or distribute this file
    as long as the above copyright notice remains

 THIS SOFTWARE IS PROVIDED "AS IS".  NO WARRANTIES, WHETHER EXPRESS, IMPLIED
 OR STATUTORY, INCLUDING, BUT NOT LIMITED TO, IMPLIED WARRANTIES OF
 MERCHANTABILITY AND FITNESS FOR A PARTICULAR PURPOSE APPLY TO THIS SOFTWARE.
 VALVANO SHALL NOT, IN ANY CIRCUMSTANCES, BE LIABLE FOR SPECIAL, INCIDENTAL,
 OR CONSEQUENTIAL DAMAGES, FOR ANY REASON WHATSOEVER.
 For more information about my classes, my research, and my books, see
 http://users.ece.utexas.edu/~valvano/
 */
// This code is derived from ethernet.c - Driver for the Integrated Ethernet Controller
// Copyright (c) 2006-2010 Texas Instruments Incorporated.  All rights reserved.
// Software License Agreement
// Texas Instruments (TI) is supplying this software for use solely and
// exclusively on TI's microcontroller products. The software is owned by
// TI and/or its suppliers, and is protected under applicable copyright
// laws. You may not combine this software with "viral" open-source
// software in order to form a larger program.

// Format of the data in the RX FIFO is as follows:
// +---------+----------+----------+----------+----------+
// |         | 31:24    | 23:16    | 15:8     | 7:0      |
// +---------+----------+----------+----------+----------+
// | Word 0  | DA 2     | DA 1     | FL MSB   | FL LSB   |
// +---------+----------+----------+----------+----------+
// | Word 1  | DA 6     | DA 5     | DA 4     | DA 3     |
// +---------+----------+----------+----------+----------+
// | Word 2  | SA 4     | SA 3     | SA 2     | SA 1     |
// +---------+----------+----------+----------+----------+
// | Word 3  | FT LSB   | FT MSB   | SA 6     | SA 5     |
// +---------+----------+----------+----------+----------+
// | Word 4  | DATA 4   | DATA 3   | DATA 2   | DATA 1   |
// +---------+----------+----------+----------+----------+
// | Word 5  | DATA 8   | DATA 7   | DATA 6   | DATA 5   |
// +---------+----------+----------+----------+----------+
// | Word 6  | DATA 12  | DATA 11  | DATA 10  | DATA 9   |
// +---------+----------+----------+----------+----------+
// | ...     |          |          |          |          |
// +---------+----------+----------+----------+----------+
// | Word X  | DATA n   | DATA n-1 | DATA n-2 | DATA n-3 |
// +---------+----------+----------+----------+----------+
// | Word Y  | FCS 4    | FCS 3    | FCS 2    | FCS 1    |
// +---------+----------+----------+----------+----------+
//
// Where FL is Frame Length, (FL + DA + SA + FT + DATA + FCS) Bytes.
// Where DA is Destination (MAC) Address.
// Where SA is Source (MAC) Address.
// Where FT is Frame Type (or Frame Length for Ethernet).
// Where DATA is Payload Data for the Ethernet Frame.
// Where FCS is the Frame Check Sequence.


// Format of the data in the TX FIFO is as follows:
//
// +---------+----------+----------+----------+----------+
// |         | 31:24    | 23:16    | 15:8     | 7:0      |
// +---------+----------+----------+----------+----------+
// | Word 0  | DA 2     | DA 1     | PL MSB   | PL LSB   |
// +---------+----------+----------+----------+----------+
// | Word 1  | DA 6     | DA 5     | DA 4     | DA 3     |
// +---------+----------+----------+----------+----------+
// | Word 2  | SA 4     | SA 3     | SA 2     | SA 1     |
// +---------+----------+----------+----------+----------+
// | Word 3  | FT LSB   | FT MSB   | SA 6     | SA 5     |
// +---------+----------+----------+----------+----------+
// | Word 4  | DATA 4   | DATA 3   | DATA 2   | DATA 1   |
// +---------+----------+----------+----------+----------+
// | Word 5  | DATA 8   | DATA 7   | DATA 6   | DATA 5   |
// +---------+----------+----------+----------+----------+
// | Word 6  | DATA 12  | DATA 11  | DATA 10  | DATA 9   |
// +---------+----------+----------+----------+----------+
// | ...     |          |          |          |          |
// +---------+----------+----------+----------+----------+
// | Word X  | DATA n   | DATA n-1 | DATA n-2 | DATA n-3 |
// +---------+----------+----------+----------+----------+
//
// Where PL is Payload Length, (DATA) only
// Where DA is Destination (MAC) Address
// Where SA is Source (MAC) Address
// Where FT is Frame Type (or Frame Length for Ethernet)
// Where DATA is Payload Data for the Ethernet Frame

// Initialize Ethernet device, no interrupts
// Inputs: Pointer to 6 bytes MAC address
// Outputs: none
// Will hang if no lock occurs
void MAC_Init(unsigned char *pucMACAddr);

// Receive an Ethernet packet
// Inputs: pointer to a buffer
//         size of the buffer
// Outputs: positive size if received
//          negative number if received but didn't fit
// Will wait if no input
long MAC_ReceiveBlocking(unsigned char *pucBuf, long lBufLen);

// Receive an Ethernet packet
// Inputs: pointer to a buffer
//         size of the buffer
// Outputs: positive size if received
//          negative number if received but didn't fit
// Will not wait if no input (returns 0 if no input)
long MAC_ReceiveNonBlocking(unsigned char *pucBuf, long lBufLen);

//*****************************************************************************
// Nonblocking Send a packet to the Ethernet controller.
//
// Inputs: pucBuf is the pointer to the packet buffer.
//         lBufLen is number of bytes in the packet to be transmitted.
// Output: 0 if Ethernet controller is busy
//         negated packet length -lBufLen if the packet is too large
//         positive packet length lBufLen if ok.
// Puts a packet into the transmit FIFO of the controller.
//
// The function will not wait for the transmission to complete.  
long MAC_TransmitNonBlocking( unsigned char *pucBuf, long lBufLen);

//*****************************************************************************
// Blocking Send a packet to the Ethernet controller.
// If busy, this function will wait for the controller
// Inputs: pucBuf is the pointer to the packet buffer.
//         lBufLen is number of bytes in the packet to be transmitted.
// Output: negated packet length -lBufLen if the packet is too large
//         positive packet length lBufLen if ok.
// Puts a packet into the transmit FIFO of the controller.
//
// The function will not wait for the transmission to complete.  
long MAC_TransmitBlocking(unsigned char *pucBuf, long lBufLen);

//*****************************************************************************
// Blocking Send data to the Ethernet controller.
// If busy, this function will wait for the controller
// Inputs: pucBuf is the pointer to data
//         lBufLen is number of bytes in the packet to be transmitted.
//         pucMACAddr[6] is the 48-bit destination address
// Output: negated packet length -lBufLen if the packet is too large
//         positive packet length lBufLen if ok.
// Puts a packet into the transmit FIFO of the controller.
//
// The function will not wait for the transmission to complete.  
long MAC_SendData(unsigned char *pucBuf, long lBufLen, unsigned char *pucMACAddr);


