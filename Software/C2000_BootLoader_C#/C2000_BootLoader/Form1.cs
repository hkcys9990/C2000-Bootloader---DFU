using System;
using System.Collections.Generic;
using System.ComponentModel;
using System.Data;
using System.Drawing;
using System.IO;
using System.Linq;
using System.Text;
using System.Threading.Tasks;
using System.Windows.Forms;
using System.IO.Ports;
using System.Threading;
using System.Net;


namespace C2000_BootLoader
{
    public partial class Form1 : Form
    {
        C2000_SerialCommunication BootloaderCmd;
        const int C2000booloader_base_address = 0xE000;
        int C2000bootloader_state_address = C2000booloader_base_address + 0;
        int C2000bootloader_error_address = C2000booloader_base_address + 2;
        int C2000bootloader_entry_trigger_address = C2000booloader_base_address + 4;
        int C2000bootloader_entry_address = C2000booloader_base_address + 6;
        int C2000bootloader_erase_trigger_address = C2000booloader_base_address + 8;
        int C2000bootloader_erase_buffer_length_address = C2000booloader_base_address + 0x0a;
        int C2000bootloader_erase_start_address_buffer_address = C2000booloader_base_address + 0x0c;
        int C2000bootloader_erase_data_length_buffer_address = C2000booloader_base_address + 0x4c;
        int C2000bootloader_write_trigger_address = C2000booloader_base_address + 0x8c;
        int C2000bootloader_write_address_pointer_address = C2000booloader_base_address + 0x8e;
        int C2000bootloader_write_buffer_length_address = C2000booloader_base_address + 0x90;
        int C2000bootloader_write_buffer_address = C2000booloader_base_address + 0x92;
        int C2000bootloader_dfu_mode_address = C2000booloader_base_address + 0x498;

        public Form1()
        {
            InitializeComponent();
            BootloaderCmd = new C2000_SerialCommunication();
            progressBar1.Minimum = 0;
            progressBar1.Maximum = 100;
            this.Text = "C2000 DFU Tool - Peter Tsang Electronics";
            this.FormBorderStyle = FormBorderStyle.FixedSingle;
            this.MaximumSize = this.MinimumSize = this.Size;
        }

        private void btn_browse_Click(object sender, EventArgs e)
        {
            OpenFileDialog openFileDialog1 = new OpenFileDialog
            {
                Title = "Browse Text Files",

                CheckFileExists = true,
                CheckPathExists = true,

                DefaultExt = "txt",
                Filter = "txt files (*.txt)|*.txt",
                FilterIndex = 2,
                RestoreDirectory = true,

                ReadOnlyChecked = true,
                ShowReadOnly = true
            };
            if (openFileDialog1.ShowDialog() == DialogResult.OK)
            {
                tb_FWpath.Text = openFileDialog1.FileName;

            }
        }
        private async void button1_Click(object sender, EventArgs e)
        {
            button1.Enabled = false; // disable button before DFU finish
            progressBar1.Value = 0;

            try
            {
                await Task.Run(async () => await PerformDfuUpdateAsync());
            }
            catch (Exception ex)
            {
                this.Invoke((MethodInvoker)delegate {
                    MessageBox.Show($"DFU fail: {ex.Message}");
                });
            }
            finally
            {
                this.Invoke((MethodInvoker)delegate {
                    button1.Enabled = true;
                });
            }
        }
        private async Task PerformDfuUpdateAsync()
        {
            try
            {
                string content;
                int Entry_point;
                int address_offset = 0;
                int TotalBlockNumber = 0;
                int[] BlockAddress = new int[100];
                int[] BlockLength = new int[100];
                int[,] BlockData = new int[100, 800000];

                using (StreamReader sr = new StreamReader(tb_FWpath.Text))
                {
                    content = sr.ReadToEnd();
                    content = content.Remove(0, 3);
                    content = content.Replace(" ", "");
                    content = content.Replace("\r", "");
                    content = content.Replace("\n", "");
                    content = content.Remove(content.Length - 1, 1);

                    if (content.Length % 2 == 0)
                    {
                        int[] b = new int[content.Length / 2];
                        for (int j = 0; j < content.Length / 2; j++)
                        {
                            b[j] = Convert.ToInt32(content.Substring(2 * j, 2), 16);
                        }

                        if (b[0] == 0xAA || b[1] == 0x08)
                        {
                            Entry_point = b[21] * 256 + b[20] + b[18] * 65536 + b[19] * 16777216;
                            address_offset = 21;

                            while (address_offset < b.Length - 2 - 1)
                            {
                                BlockLength[TotalBlockNumber] = b[address_offset + 1] + b[address_offset + 2] * 256;
                                BlockAddress[TotalBlockNumber] = b[address_offset + 3] * 65536 + b[address_offset + 4] * 16777216 + b[address_offset + 5] + b[address_offset + 6] * 256;

                                for (int i = 0; i < BlockLength[TotalBlockNumber]; i++)
                                {
                                    BlockData[TotalBlockNumber, i] = b[address_offset + 7 + (2 * i)] + b[address_offset + 7 + (2 * i + 1)] * 256;
                                }

                                address_offset = address_offset + 2 + 4 + BlockLength[TotalBlockNumber] * 2;
                                TotalBlockNumber++;
                            }

                            if (TotalBlockNumber > 100 || BlockLength[TotalBlockNumber] > 40000)
                            {
                                throw new Exception("Invalid block configuration");
                            }

                            BootloaderCmd.ResetSerialBuffer();

                            int data = await BootloaderCmd.ReadCmdAsync(C2000bootloader_state_address);
                            if (data == -1)
                            {
                                throw new Exception("Failed to read state");
                            }

                            if (data == 0x00) // IDLE
                            {
                                if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_dfu_mode_address, 0x01))
                                {
                                    throw new Exception("Failed to set DFU mode");
                                }

                                if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_entry_address, Entry_point))
                                {
                                    throw new Exception("Failed to set entry address");
                                }

                                if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_erase_buffer_length_address, TotalBlockNumber))
                                {
                                    throw new Exception("Failed to set erase buffer length");
                                }

                                for (int i = 0; i < TotalBlockNumber; i++)
                                {
                                    if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_erase_start_address_buffer_address + 2 * i, BlockAddress[i]))
                                    {
                                        throw new Exception($"Failed to set erase address for block {i}");
                                    }

                                    if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_erase_data_length_buffer_address + 2 * i, BlockLength[i]))
                                    {
                                        throw new Exception($"Failed to set erase length for block {i}");
                                    }
                                }

                                if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_erase_trigger_address, 0x01))
                                {
                                    throw new Exception("Erase trigger failed");
                                }

                                for (int r = 0; r < 10; r++)
                                {
                                    await Task.Delay(10);
                                    data = await BootloaderCmd.ReadCmdAsync(C2000bootloader_state_address);
                                    if (data == 0x00) // IDLE
                                    {
                                        break;
                                    }
                                }

                                int[] BlockWriteOrder = new int[TotalBlockNumber];
                                int[] indices = BlockAddress
                                    .Select((value, index) => new { value, index })
                                    .Where(x => x.value != 0)
                                    .OrderBy(x => x.value)
                                    .Select(x => x.index)
                                    .ToArray();

                                for (int i = 0; i < TotalBlockNumber; i++)
                                {
                                    int max_write_num = 512;
                                    int buffer_write_count = (BlockLength[indices[i]] + max_write_num - 1) / max_write_num;
                                    int remain_write_data_length = BlockLength[indices[i]];
                                    int data_written;

                                    for (int j = 0; j < buffer_write_count; j++)
                                    {
                                        if (remain_write_data_length >= max_write_num)
                                        {
                                            data_written = max_write_num;
                                            if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_address_pointer_address, BlockAddress[indices[i]] + j * max_write_num))
                                            {
                                                throw new Exception($"Write address pointer failed (block {i}, chunk {j})");
                                            }

                                            if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_buffer_length_address, max_write_num))
                                            {
                                                throw new Exception($"Write buffer length failed (block {i}, chunk {j})");
                                            }

                                            for (int k = 0; k < max_write_num; k++)
                                            {
                                                if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_buffer_address + 2 * k, BlockData[indices[i], j * max_write_num + k]))
                                                {
                                                    throw new Exception($"Write data failed (block {i}, chunk {j}, word {k})");
                                                }
                                            }

                                            remain_write_data_length = remain_write_data_length - max_write_num;
                                        }
                                        else
                                        {
                                            data_written = remain_write_data_length;
                                            if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_address_pointer_address, BlockAddress[indices[i]] + j * max_write_num))
                                            {
                                                throw new Exception($"Write address pointer failed (block {i}, chunk {j})");
                                            }

                                            if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_buffer_length_address, remain_write_data_length))
                                            {
                                                throw new Exception($"Write buffer length failed (block {i}, chunk {j})");
                                            }

                                            for (int k = 0; k < remain_write_data_length; k++)
                                            {
                                                if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_buffer_address + 2 * k, BlockData[indices[i], j * max_write_num + k]))
                                                {
                                                    throw new Exception($"Write data failed (block {i}, chunk {j}, word {k})");
                                                }
                                            }
                                        }

                                        if (!await BootloaderCmd.WriteCmdAsync(C2000bootloader_write_trigger_address, 0x01))
                                        {
                                            throw new Exception($"Write trigger failed (block {i}, chunk {j})");
                                        }

                                        for (int r = 0; r < 10; r++)
                                        {
                                            data = await BootloaderCmd.ReadCmdAsync(C2000bootloader_state_address);
                                            if (data == 0x00) // IDLE
                                            {
                                                break;
                                            }
                                        }
                                        
                                        for (int l = 0; l < data_written / 2; l++)
                                        {
                                            int flash_data = await BootloaderCmd.ReadCmdAsync(BlockAddress[indices[i]] + 2 * l);
                                            int send_data = BlockData[indices[i], 2 * l] + BlockData[indices[i], 2 * l + 1] * 65536;

                                            if ((uint)send_data != (uint)flash_data)
                                            {
                                                throw new Exception($"Verification failed at address 0x{(BlockAddress[indices[i]] + 2 * l):X8}");
                                            }
                                        }
                                        
                                    }

                                    // updateh progress bar
                                    int progress = (i + 1) * 100 / TotalBlockNumber;
                                    this.Invoke((MethodInvoker)delegate {
                                        progressBar1.Value = progress;
                                    });
                                }

                                // enter to application fw. bootloader fw will not reply
                                BootloaderCmd.WriteCmdWithoutResponse(C2000bootloader_entry_trigger_address, 0x01);
                            }
                            else
                            {
                                throw new Exception("Device not in IDLE state");
                            }
                        }
                    }
                }
            }
            catch (Exception ex)
            {
                throw new Exception($"DFU fail: {ex.Message}");
            }
        }

        private void btn_refresh_Click(object sender, EventArgs e)
        {
            string[] ports = SerialPort.GetPortNames();
            cb_seriallist.Items.Clear();
            for (int i = 0; i < ports.Length; i++)
            {
                cb_seriallist.Items.Add(ports[i]);
            }
        }
        private void btn_connect_Click(object sender, EventArgs e)
        {
            if (btn_connect.Text == "connect")
            {
                try
                {
                    if (!BootloaderCmd.IsOpened())
                    {
                        BootloaderCmd.SerialOpen(cb_seriallist.Text);
                        btn_connect.Text = "connected";
                    }
                    else
                    {
                        BootloaderCmd.SerialClose();
                        btn_connect.Text = "connect";
                    }
                }
                catch
                {
                    MessageBox.Show("select COM port");
                }
            }
            else
            {
                if (BootloaderCmd.IsOpened())
                {
                    try
                    {
                        BootloaderCmd.SerialClose();
                    }
                    catch (Exception ex)
                    {
                        MessageBox.Show(ex.Message);
                    }
                }
                btn_connect.Text = "connect";
            }
        }
        private void Form1_FormClosing(object sender, FormClosingEventArgs e)
        {
            try
            {
                BootloaderCmd.Dispose();
            }
            catch (Exception ex)
            {
                MessageBox.Show(ex.Message);
            }
        }
    }
}
