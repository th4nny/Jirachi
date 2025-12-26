public class TestClass {

    // Static initializer - runs when class is loaded
    static {
        System.out.println("[Java] TestClass static initializer running...");
        // Give the agent time to install hooks during class loading
        try {
            Thread.sleep(2000);
        } catch (InterruptedException e) {
            e.printStackTrace();
        }
        System.out.println("[Java] TestClass initialization complete");
    }

    public String getMessage() {
        return "Original Message";
    }

    public int calculate(int a, int b) {
        return a + b;
    }

    public static void main(String[] args) {
        System.out.println("\n========================================");
        System.out.println("JNIHook Test - Starting");
        System.out.println("========================================");

        TestClass obj = new TestClass();

        System.out.println("\n--- First call (should be hooked) ---");
        String msg1 = obj.getMessage();
        int calc1 = obj.calculate(10, 20);
        System.out.println("getMessage(): '" + msg1 + "'");
        System.out.println("calculate(10, 20): " + calc1);

        System.out.println("\n--- Second call (to verify consistency) ---");
        String msg2 = obj.getMessage();
        int calc2 = obj.calculate(5, 7);
        System.out.println("getMessage(): '" + msg2 + "'");
        System.out.println("calculate(5, 7): " + calc2);

        // Verify hooks worked
        System.out.println("\n========================================");
        System.out.println("Test Results:");
        System.out.println("========================================");

        boolean getMessageHooked = "Hooked Message!".equals(msg1) && "Hooked Message!".equals(msg2);
        boolean calculateHooked = (calc1 == 200) && (calc2 == 35); // 10*20=200, 5*7=35

        System.out.println("getMessage() hook: " + (getMessageHooked ? "✓ PASS" : "✗ FAIL"));
        System.out.println("  First call:  '" + msg1 + "' (expected: 'Hooked Message!')");
        System.out.println("  Second call: '" + msg2 + "' (expected: 'Hooked Message!')");
        System.out.println("calculate() hook: " + (calculateHooked ? "✓ PASS" : "✗ FAIL"));
        System.out.println("  calculate(10,20) = " + calc1 + " (expected: 200)");
        System.out.println("  calculate(5,7) = " + calc2 + " (expected: 35)");

        if (getMessageHooked && calculateHooked) {
            System.out.println("\n✓ All hooks working correctly!");
            System.exit(0);
        } else {
            System.out.println("\n✗ Some hooks failed!");
            System.exit(1);
        }
    }
}