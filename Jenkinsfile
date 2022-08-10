pipeline {
	agent none
	triggers {
		pollSCM('TZ=America/Toronto\nH/10 * * * *')
		//Polls every 10 minutes
	}
	options {
		ansiColor('xterm')
	}
	stages {
		stage('build') {
			agent {
				label 'citests'
			}
			steps {
				build job: 'firmware-compile', parameters: [string(name: 'SERVER_TARGET', value: 'all')]
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
