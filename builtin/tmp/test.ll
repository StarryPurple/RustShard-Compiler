; ModuleID = 'test.c'
source_filename = "test.c"
target datalayout = "e-m:e-p270:32:32-p271:32:32-p272:64:64-i64:64-i128:128-f80:128-n8:16:32:64-S128"
target triple = "x86_64-pc-linux-gnu"

%Food = type { i32, i32 }

define i64 @better(i64 %0, i64 %1) #0 {
  %3 = alloca %Food
  %4 = alloca %Food
  %5 = alloca %Food
  store i64 %0, ptr %4
  store i64 %1, ptr %5
  %6 = getelementptr %Food, ptr %4, i32 0, i32 0
  %7 = load i32, ptr %6
  %8 = getelementptr %Food, ptr %5, i32 0, i32 0
  %9 = load i32, ptr %8
  %10 = add nsw i32 %7, %9
  %11 = getelementptr %Food, ptr %3, i32 0, i32 0
  store i32 %10, ptr %11
  %12 = getelementptr %Food, ptr %4, i32 0, i32 1
  %13 = load i32, ptr %12
  %14 = getelementptr %Food, ptr %5, i32 0, i32 1
  %15 = load i32, ptr %14
  %16 = add nsw i32 %13, %15
  %17 = getelementptr %Food, ptr %3, i32 0, i32 1
  store i32 %16, ptr %17
  %18 = load i64, ptr %3
  ret i64 %18
}

attributes #0 = { noinline nounwind optnone uwtable "frame-pointer"="all" "min-legal-vector-width"="0" "no-trapping-math"="true" "stack-protector-buffer-size"="8" "target-cpu"="x86-64" "target-features"="+cmov,+cx8,+fxsr,+mmx,+sse,+sse2,+x87" "tune-cpu"="generic" }

!llvm.module.flags = !{!0, !1, !2, !3, !4}
!llvm.ident = !{!5}

!0 = !{i32 1, !"wchar_size", i32 4}
!1 = !{i32 8, !"PIC Level", i32 2}
!2 = !{i32 7, !"PIE Level", i32 2}
!3 = !{i32 7, !"uwtable", i32 2}
!4 = !{i32 7, !"frame-pointer", i32 2}
!5 = !{!"Ubuntu clang version 21.1.8 (++20251221032922+2078da43e25a-1~exp1~20251221153059.70)"}
