/**
 * ***************************************************************
 * <p/>
 * Copyright 2016 Samsung Electronics All Rights Reserved.
 * <p/>
 * <p/>
 * <p/>
 * Licensed under the Apache License, Version 2.0 (the "License");
 * you may not use this file except in compliance with the License.
 * You may obtain a copy of the License at
 * <p/>
 * http://www.apache.org/licenses/LICENSE-2.0
 * <p/>
 * Unless required by applicable law or agreed to in writing, software
 * distributed under the License is distributed on an "AS IS" BASIS,
 * WITHOUT WARRANTIES OR CONDITIONS OF ANY KIND, either express or implied.
 * See the License for the specific language governing permissions and
 * limitations under the License.
 * <p/>
 * ****************************************************************
 */

package org.iotivity.service.easysetup;

import android.app.Activity;
import android.content.Context;
import android.content.SharedPreferences;
import android.os.Bundle;
import android.preference.PreferenceManager;
import android.util.Log;
import android.view.View;
import android.widget.ArrayAdapter;
import android.widget.Button;
import android.widget.EditText;
import android.widget.LinearLayout;
import android.widget.RadioButton;
import android.widget.RadioGroup;
import android.widget.Spinner;
import android.widget.TextView;
import android.widget.Toast;
import android.widget.ToggleButton;

import org.iotivity.base.ModeType;
import org.iotivity.base.OcConnectivityType;
import org.iotivity.base.OcException;
import org.iotivity.base.OcPlatform;
import org.iotivity.base.OcProvisioning;
import org.iotivity.base.OcResource;
import org.iotivity.base.PlatformConfig;
import org.iotivity.base.QualityOfService;
import org.iotivity.base.ServiceType;
import org.iotivity.service.easysetup.mediator.CloudProp;
import org.iotivity.service.easysetup.mediator.CloudPropProvisioningCallback;
import org.iotivity.service.easysetup.mediator.CloudPropProvisioningStatus;
import org.iotivity.service.easysetup.mediator.DeviceConfig;
import org.iotivity.service.easysetup.mediator.DeviceProp;
import org.iotivity.service.easysetup.mediator.DevicePropProvisioningCallback;
import org.iotivity.service.easysetup.mediator.DevicePropProvisioningStatus;
import org.iotivity.service.easysetup.mediator.ESException;
import org.iotivity.service.easysetup.mediator.EasySetup;
import org.iotivity.service.easysetup.mediator.EnrolleeConf;
import org.iotivity.service.easysetup.mediator.GetConfigurationCallback;
import org.iotivity.service.easysetup.mediator.GetConfigurationStatus;
import org.iotivity.service.easysetup.mediator.RemoteEnrollee;
import org.iotivity.service.easysetup.mediator.SecurityProvisioningCallback;
import org.iotivity.service.easysetup.mediator.SecurityProvisioningStatus;
import org.iotivity.service.easysetup.mediator.WiFiConfig;
import org.iotivity.service.easysetup.mediator.enums.ESCloudProvState;
import org.iotivity.service.easysetup.mediator.enums.ESResult;
import org.iotivity.service.easysetup.mediator.enums.WIFI_AUTHTYPE;
import org.iotivity.service.easysetup.mediator.enums.WIFI_ENCTYPE;
import org.iotivity.service.easysetup.mediator.enums.WIFI_FREQ;
import org.iotivity.service.easysetup.mediator.enums.WIFI_MODE;

import java.io.File;
import java.io.FileNotFoundException;
import java.io.FileOutputStream;
import java.io.IOException;
import java.io.InputStream;
import java.io.OutputStream;
import java.util.ArrayList;
import java.util.EnumSet;


public class EasysetupActivity extends Activity {
    private static final String TAG = "Easysetup Mediator: ";
    PlatformConfig cfg;

    private static final int BUFFER_SIZE = 1024;

    private String filePath = "";
    public static final String OIC_CLIENT_JSON_DB_FILE =  "oic_svr_db_client.dat";
    public static final String OIC_SQL_DB_FILE =  "PDM.db";

    private boolean isSecurityEnabled = false;
    private boolean isFirstTime = true;

    ToggleButton mSecurityMode;

    RadioGroup mEasysetupProcess;
    RadioButton mConfigureSecProcess;
    RadioButton mGetConfigurationProcess;
    RadioButton mProvisionDevPropProcess;
    RadioButton mProvisionCloudPropProcess;

    Button mDiscoverResource;
    Button mStartGetConfiguration;
    Button mStartConfigureSec;
    Button mStartProvisionDevProp;
    Button mStartProvisionCloudProp;

    TextView mGetconfigurationStateText;
    TextView mDevNameText;
    TextView mLanguageText;
    TextView mCountryText;
    TextView mWifiModeText;
    TextView mWifiFreqText;
    TextView mCloudAccessableText;
    TextView mSecStateText;
    TextView mSecDevIDText;
    TextView mProvisionDevPropState;
    TextView mProvisionCloudPropState;

    EditText mEnrollerSsidText;
    EditText mEnrollerPWText;
    EditText mInputLanguageText;
    EditText mInputCountryText;
    EditText mAuthCodeText;
    EditText mAuthProviderText;
    EditText mCIServerText;

    LinearLayout mGetConfigurationInfo;
    LinearLayout mConfigureSecInfo;
    LinearLayout mProvisionDevPropInfo;
    LinearLayout mProvisionCloudPropInfo;

    Spinner mAuthType;
    Spinner mEncType;

    EasySetup mEasySetup;
    RemoteEnrollee mRemoteEnrollee;

    Activity mActivity;
    Context mContext;

    @Override
    protected void onCreate(Bundle savedInstanceState) {
        super.onCreate(savedInstanceState);
        setContentView(R.layout.easysetup_main);

        mActivity = EasysetupActivity.this;
        mContext = mActivity.getBaseContext();

        mSecurityMode = (ToggleButton) findViewById(R.id.btn_Security);

        mEasysetupProcess = (RadioGroup) findViewById(R.id.rg_EasysetupProcess);

        mConfigureSecProcess = (RadioButton) findViewById(R.id.btn_configurSec);
        mGetConfigurationProcess = (RadioButton) findViewById(R.id.btn_getConfiguration);
        mProvisionDevPropProcess = (RadioButton) findViewById(R.id.btn_provisionDevConf);
        mProvisionCloudPropProcess =
                (RadioButton) findViewById(R.id.btn_provisionCloudConf);

        mDiscoverResource = (Button) findViewById(R.id.btn_discoverResource);
        mStartGetConfiguration =
                (Button) findViewById(R.id.btn_startGetConfiguration);
        mStartConfigureSec = (Button) findViewById(R.id.btn_startConfigureSec);
        mStartProvisionDevProp = (Button) findViewById(R.id.btn_startProvisionDevConf);
        mStartProvisionCloudProp = (Button) findViewById(R.id.btn_startProvisionCloudConf);

        mGetconfigurationStateText =
                (TextView) findViewById(R.id.txt_getConfigurationState);
        mDevNameText = (TextView) findViewById(R.id.txt_devName);
        mLanguageText = (TextView) findViewById(R.id.txt_language);
        mCountryText = (TextView) findViewById(R.id.txt_country);
        mWifiModeText = (TextView) findViewById(R.id.txt_wifiMode);
        mWifiFreqText = (TextView) findViewById(R.id.txt_wifiFreq);
        mCloudAccessableText = (TextView) findViewById(R.id.txt_cloudAccessable);
        mSecStateText = (TextView) findViewById(R.id.txt_secState);
        mSecDevIDText = (TextView) findViewById(R.id.txt_secDevID);
        mProvisionDevPropState = (TextView) findViewById(R.id.txt_provisionDevConfState);
        mProvisionCloudPropState =
                (TextView) findViewById(R.id.txt_provisionCloudConfState);

        mEnrollerSsidText = (EditText) findViewById(R.id.editText_EnrollerSSID);
        mEnrollerPWText = (EditText) findViewById(R.id.editText_EnrollerPW);
        mInputLanguageText = (EditText) findViewById(R.id.editText_Language);
        mInputCountryText = (EditText) findViewById(R.id.editText_Country);
        mAuthCodeText = (EditText) findViewById(R.id.editText_authcode);
        mAuthProviderText = (EditText) findViewById(R.id.editText_authprovider);
        mCIServerText = (EditText) findViewById(R.id.editText_ciserver);

        mGetConfigurationInfo =
                (LinearLayout) findViewById(R.id.layout_GetConfiguration);
        mConfigureSecInfo = (LinearLayout) findViewById(R.id.layout_ConfigurSec);
        mProvisionDevPropInfo = (LinearLayout) findViewById(R.id.layout_ProvisionDevConf);
        mProvisionCloudPropInfo = (LinearLayout) findViewById(R.id.layout_ProvisionCloudConf);

        mAuthType = (Spinner) findViewById(R.id.spinner_authType);
        mEncType = (Spinner) findViewById(R.id.spinner_encType);

        mEasysetupProcess.setOnCheckedChangeListener(new RadioGroup.OnCheckedChangeListener() {
            @Override
            public void onCheckedChanged(RadioGroup group, int checkedId) {
                mGetConfigurationInfo.setVisibility(View.GONE);
                mConfigureSecInfo.setVisibility(View.GONE);
                mProvisionDevPropInfo.setVisibility(View.GONE);
                mProvisionCloudPropInfo.setVisibility(View.GONE);

                switch (checkedId) {
                    case R.id.btn_configurSec:
                        mConfigureSecInfo.setVisibility(View.VISIBLE);
                        break;

                    case R.id.btn_getConfiguration:
                        mGetConfigurationInfo.setVisibility(View.VISIBLE);
                        break;

                    case R.id.btn_provisionDevConf:
                        mProvisionDevPropInfo.setVisibility(View.VISIBLE);
                        break;

                    case R.id.btn_provisionCloudConf:
                        mProvisionCloudPropInfo.setVisibility(View.VISIBLE);
                        break;
                }
            }
        });

        ArrayAdapter<CharSequence> adAuthType, adEnctype;

        adAuthType = ArrayAdapter.createFromResource(this, R.array.auth_type,
                android.R.layout.simple_spinner_item);
        adAuthType.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        adEnctype = ArrayAdapter.createFromResource(this, R.array.enc_type,
                android.R.layout.simple_spinner_item);
        adEnctype.setDropDownViewResource(android.R.layout.simple_spinner_dropdown_item);

        mAuthType.setAdapter(adAuthType);
        mAuthType.setSelection(0);

        mEncType.setAdapter(adEnctype);
        mEncType.setSelection(0);

        addListenerForDiscoverEnrollee();
        addListenerForStartConfigureSec();
        addListenerForStartGetConfiguration();
        addListenerForStartProvisionDevProp();
        addListenerForStartProvisionCloudProp();

        mSecurityMode.setClickable(false);
        mConfigureSecProcess.setEnabled(false);
        mGetConfigurationProcess.setEnabled(false);
        mProvisionDevPropProcess.setEnabled(false);
        mProvisionCloudPropProcess.setEnabled(false);

        mEasySetup = EasySetup.getInstance(getApplicationContext());

        initOICStack();
    }

    private void initOICStack() {
        filePath = getFilesDir().getPath() + "/";

        SharedPreferences wmbPreference = PreferenceManager.getDefaultSharedPreferences
                (getApplicationContext());
        boolean isFirstRun = wmbPreference.getBoolean("FIRSTRUN", true);
        if (isFirstRun) {
            if(!copyJsonFromAsset())
            {
                Log.e(TAG, "initOICStack error: " + "copyJsonFromAsset()");
                Toast.makeText(this,"Can't Copy DB file from asset, please retry start SampleApp.",
                    Toast.LENGTH_LONG).show();
                return;
            }
            SharedPreferences.Editor editor = wmbPreference.edit();
            editor.putBoolean("FIRSTRUN", false);
            editor.commit();
        }

        cfg = new PlatformConfig(
                this,
                ServiceType.IN_PROC,
                ModeType.CLIENT_SERVER,
                "0.0.0.0", // bind to all available interfaces
                0,
                QualityOfService.LOW, filePath + OIC_CLIENT_JSON_DB_FILE);
        try {
            /*
             * Initialize DataBase
             */

            OcPlatform.Configure(cfg);

            String sqlDbPath = getFilesDir().getAbsolutePath().replace("files", "databases") +
                    File.separator;
            File file = new File(sqlDbPath);
            //check files directory exists
            if (!(file.isDirectory())) {
                file.mkdirs();
                Log.d(TAG, "Sql db directory created at " + sqlDbPath);
            }
            Log.d(TAG, "Sql db directory exists at " + sqlDbPath);

            //SQLiteDatabase.openOrCreateDatabase(sqlDbPath+ OIC_SQL_DB_FILE, null);
            OcProvisioning.provisionInit(sqlDbPath + OIC_SQL_DB_FILE);
            mSecurityMode.setChecked(true);
        } catch (OcException e) {
            logMessage(TAG + "provisionInit error: " + e.getMessage());
            Log.e(TAG, e.getMessage());
            Toast.makeText(this,"provisionInit error: " + e.getMessage(),
                    Toast.LENGTH_LONG).show();
            mSecurityMode.setChecked(false);
            return;
        } catch (UnsatisfiedLinkError e) {

            // Note : Easy setup is built with SECURED = 0, but user still selects Security feature
            // while running the Mediator App it couldn't find "libocprovision.so".
            // As per the programmer guide, security feature should be invoked only if build is done with SECURED = 1.
            mSecurityMode.setChecked(false);
            Log.e(TAG, " Easy setup is built with secured  = 0, but executed with security feature");
            Toast.makeText(this,"Security is not enabled [Easy setup is built with SECURED = 0]",
                    Toast.LENGTH_LONG).show();
            return;
        }
    }

    OcPlatform.OnResourceFoundListener listener =
            new OcPlatform.OnResourceFoundListener() {
                @Override
                public void onFindResourceFailed(Throwable throwable, String s) {
                    Log.e(TAG, "Failed found resource, ecode: " + s);
                }
                @Override
                public void onResourceFound(OcResource ocResource) {
                    synchronized (mActivity) {
                        if(isFirstTime){
                            if (null == ocResource) {
                                Log.e(TAG, "Found resource is invalid");
                                return;
                            }

                            if(ocResource.getHost().contains("coap+tcp")) {
                                Log.d(TAG, "Recv Found resource event  from tcp port, ignoring URI : " + ocResource.getUri());
                                runOnUiThread(new Runnable() {
                                    @Override
                                    public void run() {
                                        mDiscoverResource.setEnabled(true);
                                    }
                                });
                                return;
                            }

                            // Get the resource URI
                            String resourceUri = ocResource.getUri();
                            // Get the resource host address
                            String hostAddress = ocResource.getHost();
                            Log.d(TAG,"URI of the resource: " + resourceUri);
                            Log.d(TAG,"Host address of the resource: " + hostAddress);

                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mDiscoverResource.setText("Found");
                                    mConfigureSecProcess.setEnabled(true);
                                    mGetConfigurationProcess.setEnabled(true);
                                    mProvisionDevPropProcess.setEnabled(true);
                                    mProvisionCloudPropProcess.setEnabled(true);
                                }
                            });
                            isFirstTime = false;
                            mRemoteEnrollee = mEasySetup.createRemoteEnrollee(ocResource);
                        }
                    }
                }
            };

    private void addListenerForDiscoverEnrollee() {
        mDiscoverResource.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Thread thread = new Thread() {
                    boolean result = true;
                    @Override
                    public void run() {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                mDiscoverResource.setEnabled(false);
                            }
                        });

                        try {
                            String requestUri = OcPlatform.WELL_KNOWN_QUERY + "?rt=ocf.wk.prov";
                            OcPlatform.findResource("",
                                    requestUri,
                                    EnumSet.of(OcConnectivityType.CT_DEFAULT),
                                    listener
                            );
                        }
                        catch (OcException e) {
                            e.printStackTrace();
                            result = false;
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mDiscoverResource.setEnabled(true);
                                }
                            });
                        }
                    }
                };

                thread.start();
            }
        });
    }

    private void addListenerForStartConfigureSec() {
        mStartConfigureSec.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Thread thread = new Thread() {
                    @Override
                    public void run() {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                mStartConfigureSec.setEnabled(false);
                            }
                        });

                        try {
                            mRemoteEnrollee.provisionSecurity(new SecurityProvisioningCallback() {
                                @Override
                                public void onProgress(final SecurityProvisioningStatus securityProvisioningStatus) {
                                    if(securityProvisioningStatus.getESResult() == ESResult.ES_OK) {
                                        runOnUiThread(new Runnable() {
                                            @Override
                                            public void run() {
                                                mSecStateText.setText("Success");
                                                mSecDevIDText.setText(securityProvisioningStatus.getDevUUID());
                                            }
                                        });
                                    }
                                    else {
                                        runOnUiThread(new Runnable() {
                                            @Override
                                            public void run() {
                                                mSecStateText.setText("Failed");
                                                mStartConfigureSec.setEnabled(true);
                                            }
                                        });
                                    }
                                }
                            });
                        } catch (ESException e) {
                            e.printStackTrace();
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mStartConfigureSec.setEnabled(true);
                                }
                            });
                        }
                    }
                };

                thread.start();
            }
        });
    }

    private void addListenerForStartGetConfiguration(){
        mStartGetConfiguration.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Thread thread = new Thread() {
                    @Override
                    public void run() {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                mGetconfigurationStateText.setText("Process");
                                mStartGetConfiguration.setEnabled(false);
                            }
                        });

                        try {
                            mRemoteEnrollee.getConfiguration(new GetConfigurationCallback() {
                                @Override
                                public void onProgress(GetConfigurationStatus getConfigurationStatus) {
                                    if(getConfigurationStatus.getESResult() == ESResult.ES_OK) {

                                        final EnrolleeConf enrolleeConf = getConfigurationStatus.getEnrolleeConf();
                                        runOnUiThread(new Runnable() {
                                            @Override
                                            public void run() {
                                                mGetconfigurationStateText.setText("Success");
                                                mDevNameText.setText(enrolleeConf.getDeviceName());
                                                setWifiModes(enrolleeConf.getWiFiModes());
                                                setWifiFreq(enrolleeConf.getWiFiFreq());

                                                if(enrolleeConf.isCloudAccessible()) {
                                                    mCloudAccessableText.setText("TRUE");
                                                }
                                                else {
                                                    mCloudAccessableText.setText("FALSE");
                                                }
                                            }
                                        });

                                    }
                                    else {
                                        runOnUiThread(new Runnable() {
                                            @Override
                                            public void run() {
                                                mGetconfigurationStateText.setText("Failed");
                                                mStartGetConfiguration.setEnabled(true);
                                            }
                                        });
                                    }
                                }
                            });
                        } catch (ESException e) {
                            e.printStackTrace();
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mGetconfigurationStateText.setText("Failed");
                                    mStartGetConfiguration.setEnabled(true);
                                }
                            });
                        }
                    }
                };

                thread.start();
            }
        });
    }

    private void addListenerForStartProvisionDevProp() {
        mStartProvisionDevProp.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Thread thread = new Thread() {
                    @Override
                    public void run() {
                        try {
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mProvisionDevPropState.setText("Progress");
                                    mStartProvisionDevProp.setEnabled(false);
                                }
                            });

                            String enrollerSSID = mEnrollerSsidText.getText().toString();
                            String enrollerPW = mEnrollerPWText.getText().toString();
                            WIFI_AUTHTYPE authType =
                                    WIFI_AUTHTYPE.fromInt(mAuthType.getSelectedItemPosition());
                            WIFI_ENCTYPE encType =
                                    WIFI_ENCTYPE.fromInt(mEncType.getSelectedItemPosition());
                            String inputLanguage = mInputLanguageText.getText().toString();
                            String inputCountry = mInputCountryText.getText().toString();

                            DeviceProp deviceProp = new DeviceProp();
                            deviceProp.setWiFiProp(enrollerSSID, enrollerPW, authType, encType);
                            deviceProp.setDevConfProp(inputLanguage, inputCountry);

                            mRemoteEnrollee.provisionDeviceProperties(deviceProp, new DevicePropProvisioningCallback() {
                                @Override
                                public void onProgress(DevicePropProvisioningStatus devPropProvisioningStatus) {
                                    final ESResult result = devPropProvisioningStatus.getESResult();
                                    runOnUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            if(result.equals(ESResult.ES_OK)) {
                                                mProvisionDevPropState.setText("Success");
                                            }
                                            else if(result.equals(ESResult.ES_ERROR)) {
                                                mProvisionDevPropState.setText("Failed");
                                            }
                                            else if(result.equals(ESResult.ES_UNAUTHORIZED)) {
                                                mProvisionDevPropState.setText("Failed. Need SecProv");
                                            }
                                            mStartProvisionDevProp.setEnabled(true);
                                        }
                                    });
                                }
                            });
                        } catch (ESException e) {
                            e.printStackTrace();
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mProvisionDevPropState.setText("Failed");
                                    mStartProvisionDevProp.setEnabled(true);
                                }
                            });
                        }
                    }
                };

                thread.start();
            }
        });
    }

    private void addListenerForStartProvisionCloudProp() {
        mStartProvisionCloudProp.setOnClickListener(new View.OnClickListener() {
            @Override
            public void onClick(View v) {
                Thread thread = new Thread() {
                    @Override
                    public void run() {
                        runOnUiThread(new Runnable() {
                            @Override
                            public void run() {
                                mProvisionCloudPropState.setText("Progress");
                                mStartProvisionCloudProp.setEnabled(false);
                            }
                        });

                        try {
                            String authCode = mAuthCodeText.getText().toString();
                            String authProvider = mAuthProviderText.getText().toString();
                            String ciserver = mCIServerText.getText().toString();

                            CloudProp cloudProp = new CloudProp();
                            cloudProp.setCloudProp(authCode, authProvider, ciserver);
                            cloudProp.setCloudID("f002ae8b-c42c-40d3-8b8d-1927c17bd1b3");

                            mRemoteEnrollee.provisionCloudProperties(cloudProp, new CloudPropProvisioningCallback() {
                                @Override
                                public void onProgress(CloudPropProvisioningStatus cloudProvisioningStatus) {
                                    final ESResult result = cloudProvisioningStatus.getESResult();
                                    final ESCloudProvState state = cloudProvisioningStatus.getESCloudState();
                                    runOnUiThread(new Runnable() {
                                        @Override
                                        public void run() {
                                            if(result.equals(ESResult.ES_OK)) {
                                                if(state.equals(ESCloudProvState.ES_CLOUD_ENROLLEE_FOUND)) {
                                                    mProvisionCloudPropState.setText("Found Resource");
                                                }
                                                else if(state.equals(ESCloudProvState.ES_CLOUD_PROVISIONING_SUCCESS)) {
                                                    mProvisionCloudPropState.setText("Success");
                                                }
                                            }
                                            else {
                                                if(state.equals(ESCloudProvState.ES_CLOUD_ENROLLEE_NOT_FOUND)) {
                                                    mProvisionCloudPropState.setText("Not Found Resource");
                                                }
                                                else if(state.equals(ESCloudProvState.ES_CLOUD_PROVISIONING_ERROR)) {
                                                    mProvisionCloudPropState.setText("Failed");
                                                }
                                                mStartProvisionCloudProp.setEnabled(true);
                                            }
                                        }
                                    });
                                }
                            });
                        } catch (ESException e) {
                            e.printStackTrace();
                            runOnUiThread(new Runnable() {
                                @Override
                                public void run() {
                                    mProvisionCloudPropState.setText("Failed");
                                    mStartProvisionCloudProp.setEnabled(true);
                                }
                            });
                        }
                    }
                };

                thread.start();
            }
        });
    }

    private boolean copyJsonFromAsset() {
        InputStream inputStream = null;
        OutputStream outputStream = null;
        int length;
        byte[] buffer = new byte[BUFFER_SIZE];
        try {
            inputStream = getAssets().open(OIC_CLIENT_JSON_DB_FILE);
            File file = new File(filePath);
            //check files directory exists
            if (!(file.exists() && file.isDirectory())) {
                file.mkdirs();
            }
            outputStream = new FileOutputStream(filePath + OIC_CLIENT_JSON_DB_FILE);
            while ((length = inputStream.read(buffer)) != -1) {
                outputStream.write(buffer, 0, length);
            }
        } catch (NullPointerException e) {
            logMessage(TAG + "Null pointer exception " + e.getMessage());
            Log.e(TAG, e.getMessage());
            return false;
        } catch (FileNotFoundException e) {
            logMessage(TAG + "Json svr db file not found " + e.getMessage());
            Log.e(TAG, e.getMessage());
            return false;
        } catch (IOException e) {
            logMessage(TAG + OIC_CLIENT_JSON_DB_FILE + " file copy failed");
            Log.e(TAG, e.getMessage());
            return false;
        } finally {
            if (inputStream != null) {
                try {
                    inputStream.close();
                } catch (IOException e) {
                    Log.e(TAG, e.getMessage());
                    return false;
                }
            }
            if (outputStream != null) {
                try {
                    outputStream.close();
                } catch (IOException e) {
                    Log.e(TAG, e.getMessage());
                    return false;
                }
            }
        }
        return true;
    }

    public void logMessage(String text) {

    }

    public void setWifiModes(ArrayList<WIFI_MODE> types) {
        String temp = "WIFI - ";

        for(WIFI_MODE type : types) {
            if(type.equals(WIFI_MODE.WIFI_11A)) {
                temp = temp + "11A ";
            }
            else if(type.equals(WIFI_MODE.WIFI_11B)) {
                temp = temp + "11B ";
            }
            else if(type.equals(WIFI_MODE.WIFI_11G)) {
                temp = temp + "11G ";
            }
            else if(type.equals(WIFI_MODE.WIFI_11N)) {
                temp = temp + "11N ";
            }
            else if(type.equals(WIFI_MODE.WIFI_11AC)) {
                temp = temp + "11AC ";
            }
        }
        final String modeTypes = temp;
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                mWifiModeText.setText(modeTypes);
            }
        });
    }

    public void setWifiFreq(final WIFI_FREQ freq) {
        runOnUiThread(new Runnable() {
            @Override
            public void run() {
                if(freq.equals(WIFI_FREQ.WIFI_24G)) {
                    mWifiFreqText.setText("2.4G");
                }
                else if(freq.equals(WIFI_FREQ.WIFI_5G)) {
                    mWifiFreqText.setText("5G");
                }
                else if(freq.equals(WIFI_FREQ.WIFI_BOTH)) {
                    mWifiFreqText.setText("2.4G & 5G");
                }
            }
        });
    }

    @Override
    protected void onDestroy() {
        super.onDestroy();
    }
}