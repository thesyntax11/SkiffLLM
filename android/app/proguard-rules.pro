# Keep JNI entry points and their signatures so the native library can
# resolve Java_com_skiffllm_app_SkiffNative_* symbols after shrinking.
-keep class com.skiffllm.app.SkiffNative {
    native <methods>;
}
-keep interface com.skiffllm.app.SkiffNative$Callback { *; }
-keepattributes Signature,InnerClasses,EnclosingMethod
