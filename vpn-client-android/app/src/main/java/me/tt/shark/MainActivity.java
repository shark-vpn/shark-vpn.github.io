package me.tt.shark;

import android.content.Intent;
import android.net.VpnService;
import android.support.v7.app.AppCompatActivity;
import android.os.Bundle;
import android.util.Log;

public class MainActivity extends AppCompatActivity {

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.activity_main);

        Intent intent = VpnService.prepare(this);
        if (intent != null) {
            startActivityForResult(intent, 0);
        } else {
            onActivityResult(0, RESULT_OK, null);
        }
    }

    Intent intent;

    @Override
    protected void onDestroy() {
        super.onDestroy();
        //stopService(intent);
        StopMyVPN();
    }

    public void StopMyVPN() {
        try {
            if (MyVpnService.mInterface != null) {
                MyVpnService.mInterface.close();
                MyVpnService.mInterface = null;
            }
            //isRunning = false;
        } catch (Exception e) {
            Log.e(null, "xxx");
        }
    }

    protected void onActivityResult(int request, int result, Intent data) {
        if (result == RESULT_OK) {
            intent = new Intent(this, MyVpnService.class);
            startService(intent);
        }
    }

}
