pipeline {
    agent any
    options {
        buildDiscarder(logRotator(numToKeepStr: '10'))
    }

    stages {
	stage('Build') {
	    steps {
            ircNotify notifyOnStart:true, extraMessage: "Starting Build of Paracooba."
            sh 'mkdir -p build/third_party/cadical-out/build/ && touch build/third_party/cadical-out/build/libcadical.a'
            cmakeBuild buildType: 'Release', buildDir: 'build', cleanBuild: true, installation: 'InSearchPath', steps: [[withCmake: true]]
	    }
	}
	stage('Test') {
	    steps {
            ctest arguments: '-T test --no-compress-output', installation: 'InSearchPath', workingDir: 'build'
	    }
	}
	stage('Package') {
	    steps {
            cpack arguments: '-DBUILD_NUMBER=${currentBuild.number}', installation: 'InSearchPath', workingDir: 'build'
            archiveArtifacts artifacts: 'build/packages/*.deb', fingerprint: true
	    }
	}
    }

    post {
        always {
	        archiveArtifacts (
                artifacts: 'build/Testing/**/*.xml',
                fingerprint: true
            )

            // Process the CTest xml output with the xUnit plugin
            xunit (
                testTimeMargin: '3000',
                thresholdMode: 1,
                thresholds: [
                    skipped(failureThreshold: '0'),
                    failed(failureThreshold: '0')
                ],
                tools: [CTest(
                    pattern: 'build/Testing/**/*.xml',
                    deleteOutputFiles: true,
                    failIfNotNew: false,
                    skipNoTestFiles: true,
                    stopProcessingIfError: true
                )]
            )
            ircNotify()
	    }
    }
}
