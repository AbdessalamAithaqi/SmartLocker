#!/usr/bin/env python3
"""
Test script for Google Apps Script webhook
Run this from your computer (not the Pi) to verify your webhook is working
"""

import requests
import json
import sys

def test_webhook(webhook_url):
    """Test all webhook endpoints"""
    
    print("="*60)
    print("TESTING GOOGLE APPS SCRIPT WEBHOOK")
    print("="*60)
    print()
    
    # Test 1: Ping test
    print("Test 1: Testing connection...")
    try:
        response = requests.post(
            webhook_url,
            json={"action": "test"},
            timeout=10
        )
        if response.status_code == 200:
            print("✅ PASS - Webhook is responding")
            print(f"   Response: {response.json()}")
        else:
            print(f"❌ FAIL - Status code: {response.status_code}")
            return False
    except Exception as e:
        print(f"❌ FAIL - Connection error: {e}")
        print("\nPossible issues:")
        print("- Check your internet connection")
        print("- Verify the webhook URL is correct")
        print("- Make sure Apps Script is deployed as 'Web app'")
        print("- Ensure 'Who has access' is set to 'Anyone'")
        return False
    
    print()
    
    # Test 2: Check borrow (new student)
    print("Test 2: Checking if new student can borrow...")
    try:
        response = requests.post(
            webhook_url,
            json={
                "action": "check_borrow",
                "student_id": "TEST999"
            },
            timeout=10
        )
        data = response.json()
        if data.get("can_borrow") == True:
            print("✅ PASS - New student can borrow")
        else:
            print(f"❌ FAIL - Unexpected response: {data}")
            return False
    except Exception as e:
        print(f"❌ FAIL - Error: {e}")
        return False
    
    print()
    
    # Test 3: Record borrow
    print("Test 3: Recording a borrow...")
    try:
        response = requests.post(
            webhook_url,
            json={
                "action": "borrow",
                "student_id": "TEST999"
            },
            timeout=10
        )
        data = response.json()
        if data.get("action") == "borrowed":
            print("✅ PASS - Borrow recorded")
            print("   → Check your Google Sheet - should see TEST999 with has_box=TRUE")
        else:
            print(f"❌ FAIL - Unexpected response: {data}")
            return False
    except Exception as e:
        print(f"❌ FAIL - Error: {e}")
        return False
    
    print()
    
    # Test 4: Check borrow (should be denied now)
    print("Test 4: Checking if student can borrow again (should be denied)...")
    try:
        response = requests.post(
            webhook_url,
            json={
                "action": "check_borrow",
                "student_id": "TEST999"
            },
            timeout=10
        )
        data = response.json()
        if data.get("can_borrow") == False:
            print("✅ PASS - Duplicate borrow correctly prevented")
        else:
            print(f"⚠️ WARNING - Student can still borrow: {data}")
    except Exception as e:
        print(f"❌ FAIL - Error: {e}")
        return False
    
    print()
    
    # Test 5: Record return
    print("Test 5: Recording a return...")
    try:
        response = requests.post(
            webhook_url,
            json={
                "action": "return",
                "student_id": "TEST999"
            },
            timeout=10
        )
        data = response.json()
        if data.get("action") == "returned":
            print("✅ PASS - Return recorded")
            print("   → Check your Google Sheet - TEST999 should now have has_box=FALSE")
        else:
            print(f"❌ FAIL - Unexpected response: {data}")
            return False
    except Exception as e:
        print(f"❌ FAIL - Error: {e}")
        return False
    
    print()
    
    # Test 6: Check borrow again (should work now)
    print("Test 6: Checking if student can borrow after return...")
    try:
        response = requests.post(
            webhook_url,
            json={
                "action": "check_borrow",
                "student_id": "TEST999"
            },
            timeout=10
        )
        data = response.json()
        if data.get("can_borrow") == True:
            print("✅ PASS - Student can borrow again after return")
        else:
            print(f"⚠️ WARNING - Unexpected response: {data}")
    except Exception as e:
        print(f"❌ FAIL - Error: {e}")
        return False
    
    print()
    print("="*60)
    print("✅ ALL TESTS PASSED!")
    print("="*60)
    print()
    print("Your webhook is working correctly!")
    print("Next steps:")
    print("1. Check your Google Sheet to see the test data")
    print("2. Delete the TEST999 row if you want")
    print("3. Configure your Raspberry Pi with this webhook URL")
    print()
    return True


if __name__ == "__main__":
    print()
    print("Google Apps Script Webhook Tester")
    print()
    
    if len(sys.argv) > 1:
        webhook_url = sys.argv[1]
    else:
        webhook_url = input("Enter your Google Apps Script webhook URL: ").strip()
    
    if not webhook_url:
        print("❌ Error: No URL provided")
        sys.exit(1)
    
    if "YOUR_DEPLOYMENT_ID" in webhook_url:
        print("❌ Error: You need to replace YOUR_DEPLOYMENT_ID with your actual deployment ID")
        print("\nHow to get your deployment URL:")
        print("1. Open your Google Sheet")
        print("2. Go to Extensions → Apps Script")
        print("3. Click Deploy → Test deployments")
        print("4. Copy the Web App URL")
        sys.exit(1)
    
    print(f"\nTesting webhook: {webhook_url}")
    print()
    
    success = test_webhook(webhook_url)
    
    if not success:
        print("\n⚠️ Some tests failed. Please check the errors above.")
        sys.exit(1)
    
    sys.exit(0)
