pipeline {
	agent none
	//triggers {
		//cron('0 1,13,17,21 * * *')
		//This time is in UTC. 
		//For Eastern time, in spring/summer it would be 9am, 1pm, 5pm, 9pm
		//For Eastern time, in fall/winter it should be 8am, 12pm, 4pm, 8pm
	//}
	options {
		ansiColor('xterm')
	}
	//parameters {
	//	booleanParam (
	//		defaultValue: false,
	//		description: '',
	//		name : 'FORCE_FULL_BUILD')
	//}
	stages {
		stage('build') {
			agent {
				label 'citests'
			}
			steps {
				sh './autogen.sh'
				sh 'CXX="/opt/x-tools/x-tools7h/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-c++" CC="/opt/x-tools/x-tools7h/arm-unknown-linux-gnueabihf/bin/arm-unknown-linux-gnueabihf-gcc" CFLAGS="-Wall -O3 -pipe -fomit-frame-pointer -Wall -march=armv7-a -mtune=cortex-a9 -mfpu=neon" ./configure --prefix=/usr --host=arm-unknown-linux-gnueabihf'
				sh 'make'
			}
		}
	}
	post {
		always {
			echo 'The build is finished, cleaning up workspace...'
			//might need to use deleteDir() to clean up workspace
		}
		failure {
			mail to: 'shiqi.f@pervices.com',
			subject: "Failed Pipeline: ${currentBuild.fullDisplayName}",
			body: "Something is wrong with the build ${env.BUILD_URL}"
		}
	}
}
