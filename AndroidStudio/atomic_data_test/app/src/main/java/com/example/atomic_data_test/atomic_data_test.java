/*

atomic_data: A Multibyte General Purpose Lock-Free Data Structure

This is a test of atomic_data for a smartphone.
Choose the build variant for the type of your smartphone CPU.
The guts of the test are in the atomic_data_test.cpp file.

License: Public-domain Software.

Blog post: http://alexpolt.github.io/atomic-data.html
Alexandr Poltavsky
*/
package com.example.atomic_data_test;

import android.app.Activity;
import android.os.Handler;
import android.text.method.ScrollingMovementMethod;
import android.util.Log;
import android.view.View;
import android.widget.LinearLayout;
import android.widget.TextView;
import android.widget.Button;
import android.os.Bundle;


public class atomic_data_test extends Activity
{
    @Override
    public void onCreate(Bundle savedInstanceState)
    {
        super.onCreate(savedInstanceState);

        Log.wtf( "atomic_data", "Activity onCreate" );

        tv0 = new TextView(this);
        tv0.setTextSize(tv0.getTextSize() * 1.2f);
        tv0.setText("...");
        tv0.setMovementMethod( new ScrollingMovementMethod() );

        b0 = new Button( this );
        b0.setTransformationMethod(null);
        b0.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View view) {
                b0.setEnabled(false);
                tv0.setText("running atomic_data test...");
                h0.post(new Runnable() {
                    @Override
                    public void run() {
                        String r = atomicDataTest();
                        tv0.setText( r );
                        b0.setEnabled(true);
                    }
                });
            }
        });
        b0.setText("atomic_data test");

        l0 = new LinearLayout(this);
        l0.setOrientation( l0.VERTICAL );
        l0.addView(b0);
        l0.addView(tv0);
        setContentView(l0);
    }

    @Override
    public void onStop() {
        super.onStop();
        Log.wtf( "atomic_data test", "Activity onStop" );
    }

    static Handler h0 = new Handler();
    static LinearLayout l0;
    static TextView  tv0;
    static Button b0;

    public static native String atomicDataTest();

    static {
        System.loadLibrary("atomic_data_test");
    }
}
