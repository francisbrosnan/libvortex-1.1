#!/usr/bin/python
# -*- coding: utf-8 -*-
#  PyVortex: Vortex Library Python bindings
#  Copyright (C) 2009 Advanced Software Production Line, S.L.
#
#  This program is free software; you can redistribute it and/or
#  modify it under the terms of the GNU Lesser General Public License
#  as published by the Free Software Foundation; either version 2.1
#  of the License, or (at your option) any later version.
#
#  This program is distributed in the hope that it will be useful,
#  but WITHOUT ANY WARRANTY; without even the implied warranty of
#  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the GNU
#  Lesser General Public License for more details.
#
#  You should have received a copy of the GNU Lesser General Public
#  License along with this program; if not, write to the Free
#  Software Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
#  02111-1307 USA
#  
#  You may find a copy of the license under this software is released
#  at COPYING file. This is LGPL software: you are welcome to develop
#  proprietary applications using this library without any royalty or
#  fee but returning back any change, improvement or addition in the
#  form of source code, project image, documentation patches, etc.
#
#  For commercial support on build BEEP enabled solutions contact us:
#          
#      Postal address:
#         Advanced Software Production Line, S.L.
#         C/ Antonio Suarez Nº 10, 
#         Edificio Alius A, Despacho 102
#         Alcalá de Henares 28802 (Madrid)
#         Spain
#
#      Email address:
#         info@aspl.es - http://www.aspl.es/vortex
#

# import sys for command line parsing
import sys

# import python vortex binding
import vortex

####################
# regression tests #
####################

def test_00_a_check (queue):

    a_tuple = queue.pop ()
    if not a_tuple:
        error ("Found not defined expected tuple, but found: " + a_tuple)
        return False
    if a_tuple[0] != 2 or a_tuple[1] != 3:
        error ("Expected to find differente values but found: " + str (a_tuple[0]) + ", and: " + str (a_tuple[1]))
        return False

    # get a string
    a_string = queue.pop ()
    if a_string != "This is an string":
        error ("Expected to receive string: 'This is an string', but received: " + a_string)
        return False

    # get a list
    a_list = queue.pop ()
    if len (a_list) != 4:
        error ("Expected to find list length: " + len (a_list))
        return False

    return True

def test_00_a():
    ##########
    # create a queue
    queue = vortex.AsyncQueue ()

    # call to terminate queue 
    del queue

    #########

    # create a queue
    queue = vortex.AsyncQueue ()

    # call to unref
    iterator = 0
    while iterator < 100:
        # unref 
        queue.unref ()
        
        # next operation
        iterator += 1

    # and now finish 
    del queue

    ######### now check data storage
    queue = vortex.AsyncQueue ()
    
    # push items
    queue.push (1)
    queue.push (2)
    queue.push (3)
    
    # get items
    value = queue.pop ()
    if value != 1:
        error ("Expected to find 1 but found: " + str(value))
        return False

    value = queue.pop ()
    if value != 2:
        error ("Expected to find 2 but found: " + str(value))
        return False

    value = queue.pop ()
    if value != 3:
        error ("Expected to find 3 but found: " + str(value))
        return False

    # call to unref 
    queue.unref ()

    ###### now masive add operations
    queue = vortex.AsyncQueue ()

    # add items
    iterator = 0
    while iterator < 1000:
        queue.push (iterator)
        iterator += 1

    # restore items
    iterator = 0
    while iterator < 1000:
        value = queue.pop ()
        if value != iterator:
            error ("Expected to find: " + str(value) + ", but found: " + str(iterator))
            return False
        iterator += 1

    # finish queue 
    queue.unref ()

    ##### now add different types of data
    queue = vortex.AsyncQueue ()

    queue.push ((2, 3))
    queue.push ("This is an string")
    queue.push ([1, 2, 3, 4])

    # get a tuple
    if not test_00_a_check (queue):
        return False

    # unref the queue
    queue.unref ()

    #### now add several different item
    queue    = vortex.AsyncQueue ()
    iterator = 0
    while iterator < 1000:
        
        queue.push ((2, 3))
        queue.push ("This is an string")
        queue.push ([1, 2, 3, 4])

        # next iterator
        iterator += 1

    # now retreive all items
    iterator = 0
    while iterator < 1000:
        # check queue items
        if not test_00_a_check (queue):
            return False
        
        # next iterator
        iterator += 1

    # finish the queue
    queue.unref ()

    return True

def test_01():
    # call to initilize a context and to finish it 
    ctx = vortex.Ctx ()

    # init context and finish it */
    info ("init context..")
    if not ctx.init ():
        error ("Failed to init Vortex context")
        return False

    # ok, now finish context
    info ("finishing context..")
    ctx.exit ()

    # finish ctx 
    del ctx

    return True

def test_02():
    # call to initialize a context 
    ctx = vortex.Ctx ()

    # call to init ctx 
    if not ctx.init ():
        error ("Failed to init Vortex context")
        return False

    # call to create a connection
    conn = vortex.Connection (ctx, host, port)

    # check connection status after if 
    if not conn.is_ok ():
        error ("Expected to find proper connection result, but found error. Error code was: " + str(conn.status) + ", message: " + conn.error_msg)
        return False

    info ("BEEP connection created to: " + conn.host + ":" + conn.port) 
    
    # now close the connection
    info ("Now closing the BEEP session..")
    conn.close ()

    ctx.exit ()

    # finish ctx 
    del ctx

    return True

# test connection shutdown before close.
def test_03 ():
    # call to initialize a context 
    ctx = vortex.Ctx ()

    # call to init ctx 
    if not ctx.init ():
        error ("Failed to init Vortex context")
        return False

    # call to create a connection
    conn = vortex.Connection (ctx, host, port)

    # check connection status after if 
    if not conn.is_ok ():
        error ("Expected to find proper connection result, but found error. Error code was: " + str(conn.status) + ", message: " + conn.error_msg)
        return False

    # now shutdown
    conn.shutdown ()
    
    # now close the connection (already shutted down)
    conn.close ()

    ctx.exit ()

    # finish ctx 
    del ctx

    return True

# create a channel
def test_04 ():
    # call to initialize a context 
    ctx = vortex.Ctx ()

    # call to init ctx 
    if not ctx.init ():
        error ("Failed to init Vortex context")
        return False

    # call to create a connection
    conn = vortex.Connection (ctx, host, port)

    # check connection status after if 
    if not conn.is_ok ():
        error ("Expected to find proper connection result, but found error. Error code was: " + str(conn.status) + ", message: " + conn.error_msg)
        return False

    # now create a channel
    channel     = conn.open_channel (0, REGRESSION_URI)

    if not channel:
        error ("Expected to find proper channel creation, but error found:")
        # get first message
        error = conn.pop_channel_error ()
        while error:
            error ("Found error message: " + str (error.code) + ": " + error.msg)

            # next message
            error = conn.pop_channel_error ()
        return False

    # check channel installed
    if conn.num_channels != 2:
        error ("Expected to find only two channels installed (administrative BEEP channel 0 and test channel) but found: " + conn.num_channels ())
        return False

    # now close the channel
    if not channel.close ():
        error ("Expected to find proper channel close operation, but error found: ")
        # get first message
        error = conn.pop_channel_error ()
        while error:
            error ("Found error message: " + str (error.code) + ": " + error.msg)

            # next message
            error = conn.pop_channel_error ()
        return False

    # check channel installed
    if conn.num_channels != 1:
        error ("Expected to find only one channel installed (administrative BEEP channel 0) but found: " + conn.num_channels ())
        return False
    
    # now close the connection (already shutted down)
    conn.close ()

    ctx.exit ()

    # finish ctx 
    del ctx

    return True

def test_05_received (conn, channel, frame, data):
    print ("test_05_received: Frame received, content: " + frame.payload + ", size: " + str (frame.payload_size))

    # push data received
    data.push (frame)
    
    return

# create a channel
def test_05 ():
    # call to initialize a context 
    ctx = vortex.Ctx ()

    # call to init ctx 
    if not ctx.init ():
        error ("Failed to init Vortex context")
        return False

    # call to create a connection
    conn = vortex.Connection (ctx, host, port)

    # check connection status after if 
    if not conn.is_ok ():
        error ("Expected to find proper connection result, but found error. Error code was: " + str(conn.status) + ", message: " + conn.error_msg)
        return False

    # now create a channel
    channel  = conn.open_channel (0, REGRESSION_URI)

    if not channel:
        error ("Expected to find proper channel creation, but error found:")
        # get first message
        error_data = conn.pop_channel_error ()
        while error_data:
            error ("Found error message: " + str (error_data.code) + ": " + error_data.msg)

            # next message
            error_data = conn.pop_channel_error ()
        return False

    # configure frame received handler 
    queue = vortex.AsyncQueue ()
    channel.set_frame_received (test_05_received, queue)

    # send a message to test */
    print ("test_05: Sending frame..")
    channel.send_msg ("This is a test", 14)

    # wait for the reply
    print ("test_05: Wait for reply..")
    frame = queue.pop ()

    print ("test_05: unref the async queue..")

    # finish the queue (not required)
    queue.unref ()

    print ("test_05: Checking content received..")

    # check frame content here 
    if frame.payload != "This is a test":
        error ("Received frame content '" + frame.payload + "', but expected: 'This is a test'")
        return False

    # check frame type
    if frame.type != "RPY":
        error ("Expected to receive frame type RPY but found: " + frame.type)
        return False

    print ("test_05: Closing connection..")
    
    # now close the connection (already shutted down)
    conn.close ()

    print ("test_05: finishing context..")

    ctx.exit ()

    print ("test_05: Returning from test..")

    return True

###########################
# intraestructure support #
###########################

def info (msg):
    print "[ INFO  ] : " + msg

def error (msg):
    print "[ ERROR ] : " + msg

def ok (msg):
    print "[  OK   ] : " + msg

def run_all_tests ():
    test_count = 0
    for test in tests:

        # print log
        info ("Test-" + str(test_count) + ": Running " + test[1])
        
        # call test
        if not test[0]():
            error ("detected test failure at: " + test[1])
            return False
        
        # next test
        test_count += 1
    
    ok ("All tests ok!")
    return True
        

# declare list of tests available
tests = [
#    (test_00_a, "Check PyVortex async queue wrapper"),
#    (test_01,   "Check PyVortex context initialization"),
#    (test_02,   "Check PyVortex basic BEEP connection"),
#    (test_03,   "Check PyVortex basic BEEP connection (shutdown)"),
#    (test_04,   "Check PyVortex basic BEEP channel creation"),
    (test_05,   "Check BEEP basic data exchange")
]

# declare default host and port
host     = "localhost"
port     = "44010"

# regression test beep uris
REGRESSION_URI = "http://iana.org/beep/transient/vortex-regression"

if __name__ == '__main__':
    iterator = 0
    for arg in sys.argv:
        # according to the argument position, take the value 
        if iterator == 1:
            host = arg
        elif iterator == 2:
            port = arg
            
        # next iterator
        iterator += 1

    # drop a log
    info ("Running tests against " + host + ":" + port)

    # call to run all tests
    run_all_tests ()




