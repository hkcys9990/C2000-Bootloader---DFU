using System;
using System.Collections.Concurrent;
using System.IO.Ports;
using System.Text;
using System.Threading;
using System.Threading.Tasks;
using System.Diagnostics; 
using System.Collections.Generic;  

namespace C2000_BootLoader
{
    public class C2000_SerialCommunication : IDisposable
    {
        private SerialPort _serialPort;
        private readonly ConcurrentDictionary<string, TaskCompletionSource<Frame>> _pendingRequests = new ConcurrentDictionary<string, TaskCompletionSource<Frame>>();
        private readonly object _lock = new object();
        private readonly StringBuilder _receiveBuffer = new StringBuilder(); 

        public void SerialOpen(string comPort)
        {
            if (_serialPort != null && _serialPort.IsOpen)
                return;

            _serialPort = new SerialPort(comPort, 115200)
            {
                DataBits = 8,
                StopBits = StopBits.One,
                Handshake = Handshake.None,
                ReadTimeout = 1000,
                WriteTimeout = 1000,
                ReadBufferSize = 65536, 
                WriteBufferSize = 65536
            };

            _serialPort.DataReceived += SerialDataReceived;
            _serialPort.Open();
        }

        public void SerialClose()
        {
            if (_serialPort != null)
            {
                _serialPort.DataReceived -= SerialDataReceived;
                _serialPort.Close();
                _serialPort.Dispose();
                _serialPort = null;
            }
        }

        public async Task<bool> WriteCmdAsync(int address, int data, int timeout = 1000)
        {
            var frame = new Frame
            {
                CommandType = 0x01, // write commnad
                CommandHeader = "0001", // write header
                Address = address,
                Data = data
            };

            var response = await SendCommandAsync(frame, timeout).ConfigureAwait(false);
            return response != null && response.Data == data;
        }

        public void WriteCmdWithoutResponse(int address, int data)
        {
            var frame = new Frame
            {
                CommandType = 0x01,
                CommandHeader = "0001",
                Address = address,
                Data = data
            };
            SendFrame(frame);
        }

        public async Task<int> ReadCmdAsync(int address, int timeout = 1000)
        {
            var frame = new Frame
            {
                CommandType = 0x02, // read command 
                CommandHeader = "0002", // read header
                Address = address,
                Data = 0
            };

            var response = await SendCommandAsync(frame, timeout).ConfigureAwait(false);
            return response != null ? response.Data : -1;
        }

        private async Task<Frame> SendCommandAsync(Frame frame, int timeout)
        {
            var key = $"{frame.CommandType}:{frame.Address:X8}";
            var tcs = new TaskCompletionSource<Frame>();

            lock (_lock)
            {
                _pendingRequests[key] = tcs;
            }

            try
            {
                SendFrame(frame);

                var delayTask = Task.Delay(timeout);
                var completedTask = await Task.WhenAny(tcs.Task, delayTask).ConfigureAwait(false);

                if (completedTask == delayTask)
                {
                    throw new TimeoutException("Command timed out");
                }

                return await tcs.Task.ConfigureAwait(false);
            }
            catch
            {
                return null;
            }
            finally
            {
                lock (_lock)
                {
                    TaskCompletionSource<Frame> removed;
                    _pendingRequests.TryRemove(key, out removed);
                }
            }
        }

        private void SendFrame(Frame frame)
        {
            if (_serialPort == null || !_serialPort.IsOpen)
                return;
            var command = new StringBuilder()
                .Append('\u0002')//start char
                .Append(frame.CommandHeader)  
                .Append(frame.CommandType.ToString("X2"))
                .Append("00")   //reserved
                .Append(frame.Address.ToString("X8"))
                .Append(frame.Data.ToString("X8"))
                .Append("00")    //reserved
                .Append('\u0003');//stop char

            var buffer = Encoding.ASCII.GetBytes(command.ToString());
            _serialPort.Write(buffer, 0, buffer.Length);
        }

        private void SerialDataReceived(object sender, SerialDataReceivedEventArgs e)
        {
            try
            {
                if (_serialPort.BytesToRead <= 0)
                    return;

                // read all data from PC
                var buffer = new byte[_serialPort.BytesToRead];
                int bytesRead = _serialPort.Read(buffer, 0, buffer.Length);
                string newData = Encoding.ASCII.GetString(buffer, 0, bytesRead);
                _receiveBuffer.Append(newData);

                // handle the data
                ProcessReceiveBuffer();
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"SerialDataReceived error: {ex.Message}");
            }
        }

        private void ProcessReceiveBuffer()
        {
            while (true)
            {
                string bufferContent = _receiveBuffer.ToString();

                // finding the start char
                int stxIndex = bufferContent.IndexOf('\u0002');
                if (stxIndex < 0)
                {
                    // if no start char, clear buffer
                    _receiveBuffer.Clear();
                    return;
                }

                // claer all data before the start char
                if (stxIndex > 0)
                {
                    _receiveBuffer.Remove(0, stxIndex);
                    bufferContent = _receiveBuffer.ToString();
                }

                // finding the end char
                int etxIndex = bufferContent.IndexOf('\u0003', 1); // start from 1
                if (etxIndex < 0)
                {
                    // if not yet received the end char, keep storing the data to the buffer
                    return;
                }

                // get the command package
                string frame = bufferContent.Substring(0, etxIndex + 1);
                _receiveBuffer.Remove(0, etxIndex + 1);

                // handling the package
                ProcessFrame(frame);
            }
        }

        private void ProcessFrame(string frame)
        {
            try
            {
                // checking the start and enc char
                if (!frame.StartsWith("\u0002") || !frame.EndsWith("\u0003") || frame.Length < 20)
                {
                    Debug.WriteLine($"Invalid frame: {frame}");
                    return;
                }

                string content = frame.Trim('\u0002', '\u0003');

                // decode the command type
                int commandType;
                if (!int.TryParse(content.Substring(4, 2), System.Globalization.NumberStyles.HexNumber,
                                 null, out commandType))
                {
                    Debug.WriteLine($"Failed to parse command type: {content.Substring(4, 2)}");
                    return;
                }
                // decode the address
                int address;
                if (!int.TryParse(content.Substring(8, 8), System.Globalization.NumberStyles.HexNumber,
                                 null, out address))
                {
                    Debug.WriteLine($"Failed to parse address: {content.Substring(8, 8)}");
                    return;
                }

                // decode the data
                int value;
                if (!int.TryParse(content.Substring(16, 8), System.Globalization.NumberStyles.HexNumber,
                                 null, out value))
                {
                    Debug.WriteLine($"Failed to parse data: {content.Substring(16, 8)}");
                    return;
                }

                string key = $"{commandType}:{address:X8}";

                lock (_lock)
                {
                    TaskCompletionSource<Frame> tcs;
                    if (_pendingRequests.TryGetValue(key, out tcs))
                    {
                        tcs.TrySetResult(new Frame
                        {
                            CommandType = commandType,
                            Address = address,
                            Data = value
                        });
                    }
                    else
                    {
                        Debug.WriteLine($"Unexpected response: {frame}");
                    }
                }
            }
            catch (Exception ex)
            {
                Debug.WriteLine($"Frame processing error: {ex.Message}");
            }
        }

        public void Dispose()
        {
            SerialClose();
        }

        private class Frame
        {
            public int CommandType { get; set; }
            public string CommandHeader { get; set; }
            public int Address { get; set; }
            public int Data { get; set; }
        }

        public bool IsOpened()
        {
            return _serialPort != null && _serialPort.IsOpen;
        }

        public void ResetSerialBuffer()
        {
            if (_serialPort != null && _serialPort.IsOpen)
            {
                _serialPort.DiscardInBuffer();
                _receiveBuffer.Clear();
            }
        }
    }
}

