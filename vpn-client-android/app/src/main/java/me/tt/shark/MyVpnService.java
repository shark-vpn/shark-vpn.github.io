package me.tt.shark;

import android.content.Intent;
import android.net.VpnService;
import android.os.ParcelFileDescriptor;
import android.util.Log;

import java.io.FileInputStream;
import java.io.FileOutputStream;
import java.net.*;

public class MyVpnService extends VpnService {
    static String SERVER_ADDR = "192.168.1.169";
    static int SERVER_PORT = 7194;
    static int MAX_BKG_LEN = 65535;

    static ParcelFileDescriptor mInterface;
    FileInputStream in;
    FileOutputStream out;
    DatagramSocket sock;
    InetAddress serverAddr;

    Thread mThread1;
    Thread mThread2;

    @Override
    public int onStartCommand(Intent intent, int flags, int startId) {
        Builder builder = new Builder();
        builder.setSession("MyVPNService");
        builder.addAddress("192.168.194.1", 24);
        builder.addDnsServer("8.8.8.8");
        builder.addRoute("0.0.0.0", 0);

        mInterface = builder.establish();
        in = new FileInputStream(mInterface.getFileDescriptor());
        out = new FileOutputStream(mInterface.getFileDescriptor());

        try {
            serverAddr = InetAddress.getByName(SERVER_ADDR);
            sock = new DatagramSocket();
            sock.setSoTimeout(500);
            protect(sock);
        } catch (Exception e) {
            e.printStackTrace();
            return START_NOT_STICKY;
        }

        mThread1 = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    int length;
                    byte[] ip_pkg = new byte[MAX_BKG_LEN];
                    while ((length = in.read(ip_pkg)) >= 0) {
                        if (length == 0) {
                            continue;
                        }
                        DatagramPacket msg = new DatagramPacket(
                                ip_pkg, length, serverAddr, SERVER_PORT);
                        sock.send(msg);
                    }
                    in.close();
                } catch (Exception e) {
                    Log.e(null, e.getMessage());
                }
            }
        }, "sender");

        mThread2 = new Thread(new Runnable() {
            @Override
            public void run() {
                try {
                    while (true) {
                        byte[] ip_buf = new byte[MAX_BKG_LEN];
                        DatagramPacket msg_r = new DatagramPacket(
                                ip_buf, MAX_BKG_LEN, serverAddr, SERVER_PORT);
                        try {
                            sock.receive(msg_r);
                        } catch (Exception e) {
                            continue;
                        }
                        int pkg_len = msg_r.getLength();
                        if (pkg_len > 0) {
                            out.write(ip_buf, 0, pkg_len);
                        }
                    }
                } catch (Exception e) {
                    Log.e(null, e.getMessage());
                }
            }
        }, "receiver");

        mThread1.start();
        try {
            Thread.sleep(1000);
        } catch (Exception e) {
            e.printStackTrace();
        }
        mThread2.start();
        return START_STICKY;
    }

    @Override
    public void onDestroy() {
        // TODO Auto-generated method stub
        if (mThread1 != null) {
            mThread1.interrupt();
        }
        super.onDestroy();
    }

    public void StopMyVPN() {
        try {
            if (mInterface != null) {
                mInterface.close();
                mInterface = null;
            }
            //isRunning = false;
        } catch (Exception e) {

        }
        stopSelf();
    }
}
