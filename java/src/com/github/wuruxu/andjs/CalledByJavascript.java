package com.github.wuruxu.andjs;

import java.lang.annotation.ElementType;
import java.lang.annotation.Retention;
import java.lang.annotation.RetentionPolicy;
import java.lang.annotation.Target;

/**
 *  @AccessedByNative is used to ensure proguard will keep this field, since it's
 *  only accessed by native.
 */
@Target({ElementType.CONSTRUCTOR, ElementType.METHOD})
@Retention(RetentionPolicy.CLASS)
public @interface CalledByJavascript {
    public String value() default "";
}
