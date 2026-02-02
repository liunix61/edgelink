import zmq
import time
import random
import threading

context = zmq.Context()

def source_thread():
    zmq_socket = context.socket(zmq.PUSH)
    zmq_socket.bind("inproc://source")
    # Start your result manager and workers before you start your producers
    for num in range(20000):
        work_message = { 'num' : num }
        zmq_socket.send_json(work_message)
        print("Source\t\t -> Sent")
        time.sleep(1.0)


def match_thread():
    results_receiver = context.socket(zmq.PULL)
    results_receiver.bind("inproc://sink")
    for x in range(1000):
        result = results_receiver.recv_json()
        print("Result\t\t ->%s", result)


def filter_thread():
    consumer_id = random.randrange(1,10005)
    print("I am consumer #%s" % (consumer_id))
    # recieve work
    consumer_receiver = context.socket(zmq.PULL)
    consumer_receiver.connect("inproc://source")
    # send work
    consumer_sender = context.socket(zmq.PUSH)
    consumer_sender.connect("inproc://sink")
    
    while True:
        work = consumer_receiver.recv_json()
        print("Filter\t\t -> Received")
        time.sleep(0.5)
        data = work['num']
        result = { 'consumer' : consumer_id, 'num' : data}
        if data%2 == 0: 
            consumer_sender.send_json(result)
            print("Filter\t\t -> Sent")


if __name__ == "__main__":
    source_thread = threading.Thread(target=source_thread)
    match_thread = threading.Thread(target=match_thread)
    filter_thread = threading.Thread(target=filter_thread)

    source_thread.start()
    print("source thread started.")

    filter_thread.start()
    print("filter thread started.")

    match_thread.start()
    print("match thread started.")

    time.sleep(1)


    source_thread.join()
    match_thread.join()
    filter_thread.join()
