import socket
import random
import string

def generate_random_data(length=20):
    # Generate a random string of the specified length
    return ''.join(random.choices(string.ascii_letters + string.digits, k=length))

def connect_to_host():
    host = '127.0.0.1'  # Host IP
    port = 12345         # Port to connect to

    try:
        # Create a socket object
        client_socket = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
        
        # Connect to the server
        client_socket.connect((host, port))
        
        print(f"Successfully connected to {host}:{port}")

        # Generate random data and send to the server
        random_data = generate_random_data()
        print(f"Sending random data: {random_data}")
        client_socket.sendall(random_data.encode('utf-8'))

        # Example: Receiving a response from the server
        response = client_socket.recv(1024)
        print("Received from server:", response.decode('utf-8'))

        # Close the connection
        client_socket.close()

    except ConnectionRefusedError:
        print(f"Failed to connect to {host}:{port}. Is the server running?")
    except Exception as e:
        print(f"An error occurred: {e}")

if __name__ == "__main__":
    connect_to_host()
